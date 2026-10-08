#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* Maximum length of a line in the CSV file */
#define MAX_LINE 200000


typedef struct {
    unsigned long id; 
    char *name;
    double lat, lon;
    unsigned short nsucc;
    unsigned long *successors;
} node;

void ExitError(const char *miss, int errcode) {
fprintf (stderr, "\nERROR: %s.\nStopping...\n\n", miss); exit(errcode);
}

/* Display the progress of a file-processing pass. */
static void show_progress(const char *label, long current, long total) {
    if (total <= 0) return;

    int percent = (int)((100.0 * current) / total);
    if (percent > 100) percent = 100;

    printf("\r%s: %3d%%", label, percent);
    fflush(stdout);

    if (percent == 100) {
        printf("\n");
    }
}

/* Binary search for node index by id, always looking for the value in the middle */
static long find_node_index(node *nodes, unsigned long nnodes, unsigned long id) {
    long lo = 0, hi = (long)nnodes - 1;
    while (lo <= hi) {
        long mid = (lo + hi) / 2;
        if (nodes[mid].id == id) return mid;
        else if (nodes[mid].id < id) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;
}

/* Count how many "node|" lines are in the CSV file */
static unsigned long count_nodes(const char *csv_file) {
    FILE *fin = fopen(csv_file, "r");
    if (!fin) ExitError("The .csv could not be opened in order to count the number of nodes", 1);

    fseek(fin, 0, SEEK_END);
    long file_size = ftell(fin);
    rewind(fin);

    char line[MAX_LINE];
    unsigned long nnodes = 0;
    long last_progress = -1;

    while (fgets(line, MAX_LINE, fin)) {
        if (line[0] == '#') continue;
        if (strncmp(line, "node|", 5) == 0) nnodes++;

        long current = ftell(fin);
        int percent = (file_size > 0) ? (int)((100.0 * current) / file_size) : 100;

        if (percent != last_progress) {
            show_progress("Counting nodes", current, file_size);
            last_progress = percent;
        }
    }

    fclose(fin);
    return nnodes;
}

/* Extract id, lat, lon from a "node|..." line.
Being id the value between the first and second '|' and
lat and lon being the last two fields of the line. */

static void parse_node_line(const char *line, unsigned long *id, double *lat, double *lon) {
    const char *p = line;

    if (strncmp(p, "node|", 5) != 0) {
        ExitError("parse_node_line called with a line that does not start with node|", 2);
    }
    p += 5; /* skipping "node|" */

    /* id is the string from p to the next '|' */
    /* endptr will point to the first character after the number */
    char *endptr;
    errno = 0;

    *id = strtoul(p, &endptr, 10);

    if (endptr == p){
        ExitError("Non-numeric id", 3);
    }
    if (errno == ERANGE){
        ExitError("id out of range", 4);
    }
    if (*endptr != '|'){
        ExitError("bad written id", 5);
    }
    p = endptr + 1;   /* points now to the character after '|' */

    char *buf = strdup(line);
    if (!buf) ExitError("Out of memory", 6);

    char *nl = strchr(buf, '\n');
    if (nl) *nl = '\0';

    char *last = strrchr(buf, '|');
    if (!last) ExitError("lon not found", 7);
    char *lon_str = last + 1;
    *last = '\0';

    /* Extract lat from the penultimate '|' */
    char *last2 = strrchr(buf, '|');
    if (!last2) ExitError("lat not found", 8);
    char *lat_str = last2 + 1;

    *lat = atof(lat_str);
    *lon = atof(lon_str);

    free(buf);
}

/* Write successors from a "way|..." line into nodes' successors arrays */

/* This function works in two modes:
   - If nsucc_fill is NULL, it works in COUNT mode, incrementing the nsucc counters.
   - If nsucc_fill is not NULL, it works in FILL mode, writing into successors arrays.
*/
static void process_way(const char *line, node *nodes, unsigned long nnodes, unsigned short *nsucc_fill) {
    if (strncmp(line, "way|", 4) != 0) return;

    char tmp[MAX_LINE];
    strncpy(tmp, line, MAX_LINE - 1);
    tmp[MAX_LINE - 1] = '\0';

    char *p = tmp + 4;  /* after "way|" */

    char *bar = NULL;

    /* Skip the @id field of the way */
    bar = strchr(p, '|');
    if (!bar) return;
    p = bar + 1;

    /* Skip also the @name, @place, @highway, @route and @ref fields */
    for (int k = 0; k < 5; ++k) {
        bar = strchr(p, '|');
        if (!bar) return;
        p = bar + 1;
    }

    /* Check @oneway field creating a boolean is_oneway */
    char *oneway_start = p;
    bar = strchr(p, '|');
    if (!bar) return;
    *bar = '\0';
    int is_oneway = (strcmp(oneway_start, "oneway") == 0);
    p = bar + 1;

    /* Skip @maxspeed field */
    bar = strchr(p, '|');
    if (!bar) return;
    p = bar + 1;

    long prev_idx = -1;
    int have_prev = 0;

    while (*p) {
        while (*p == '|' || *p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '\n') break;
        
        /* Find the end of the current node id and extract it making sure it ends properly */
        char *end = p;
        while (*end && *end != '|' && *end != '\n') end++;
        char save = *end;
        *end = '\0';

        /* Create numeric id and restore the character at the end */
        unsigned long nid = strtoul(p, NULL, 10);
        *end = save;
        p = end;

        /* Find the index of the node in the nodes array */
        long idx = find_node_index(nodes, nnodes, nid);
        if (idx < 0) {
            continue;
        }

        /* If we have a previous node, create the edges */
        if (have_prev) {
            if (nsucc_fill == NULL) {
                /* If in COUNT mode, just increment the nsucc counters */
                nodes[prev_idx].nsucc++;
                if (!is_oneway) {
                    nodes[idx].nsucc++;
                }
            } else {
                /* If in FILL mode, write the successors into the successors arrays */
                unsigned short pos = nodes[prev_idx].nsucc;
                if (pos < nsucc_fill[prev_idx]) {
                    nodes[prev_idx].successors[pos] = (unsigned long)idx;
                    nodes[prev_idx].nsucc++;
                }
                if (!is_oneway) {
                    /* Add the reverse edge if not oneway */
                    pos = nodes[idx].nsucc;
                    if (pos < nsucc_fill[idx]) {
                        nodes[idx].successors[pos] = (unsigned long)prev_idx;
                        nodes[idx].nsucc++;
                    }
                }
            }
        }

        prev_idx = idx;
        have_prev = 1;
    }
}

/* Build the graph from the road network CSV file and serialize it to binary. */
int main (int argc, char *argv[]){
    FILE *fin;
    unsigned long nnodes = 23895681UL;
    node *nodes;
    if((nodes = (node *) malloc(nnodes*sizeof(node))) == NULL)
        ExitError("when allocating memory for the nodes vector", 9);

    const char *csv_file = "data/spain.csv";
    const char *bin_file = "data/spain.bin";

    if (argc > 1) csv_file = argv[1];
    if (argc > 2) bin_file = argv[2];

    printf("Counting nodes in %s.\n", csv_file);
    /* First pass is to count the number of nodes */
    nnodes = count_nodes(csv_file);
    printf("Total number of nodes: %lu\n", nnodes);

    /* Second pass is to read nodes and fill the nodes array */
    fin = fopen(csv_file, "r");
    if (!fin) ExitError("Error opening the .csv file to extract the coordinates", 10);

    fseek(fin, 0, SEEK_END);
    long file_size = ftell(fin);
    rewind(fin);

    char line[MAX_LINE];
    unsigned long idx = 0;

    printf("Reading nodes and building the node vector.\n");

    long last_progress = -1;

    while (fgets(line, MAX_LINE, fin)) {
        if (line[0] == '#') continue;
        if (strncmp(line, "node|", 5) == 0) {
            unsigned long id;
            double lat, lon;
            parse_node_line(line, &id, &lat, &lon);
            nodes[idx].id = id;
            nodes[idx].lat = lat;
            nodes[idx].lon = lon;
            nodes[idx].name = NULL;
            nodes[idx].nsucc = 0;
            nodes[idx].successors = NULL;
            idx++;

            long current = ftell(fin);
            int percent = (file_size > 0) ? (int)((100.0 * current) / file_size) : 100;

            if (percent != last_progress) {
                show_progress("Reading nodes", current, file_size);
                last_progress = percent;
            }
        }
    }

    show_progress("Reading nodes", file_size, file_size);
    printf("All nodes have been read successfully.\n");
    fclose(fin);

    if (idx != nnodes) ExitError("Number of counted and read nodes do not match", 11);

    /* Third pass is to process ways to count successors */
    fin = fopen(csv_file, "r");
    if (!fin) ExitError("Error opening the .csv file to extract the number of child nodes", 12);

    fseek(fin, 0, SEEK_END);
    file_size = ftell(fin);
    rewind(fin);

    last_progress = -1;

    printf("Processing ways and counting successors.\n");

    while (fgets(line, MAX_LINE, fin)) {
        if (line[0] == '#') continue;
        if (strncmp(line, "way|", 4) == 0) {
            process_way(line, nodes, nnodes, NULL);
        }

        long current = ftell(fin);
        int percent = (file_size > 0) ? (int)((100.0 * current) / file_size) : 100;

        if (percent != last_progress) {
            show_progress("Processing ways", current, file_size);
            last_progress = percent;
        }
    }

    fclose(fin);

    /* Computing the total number of successors */
    unsigned long ntotnsucc=0UL;
    unsigned long i;
    for(i=0; i < nnodes; i++) ntotnsucc += nodes[i].nsucc;

    unsigned short *nsucc_fill = (unsigned short *)malloc(nnodes * sizeof(unsigned short));
    if (!nsucc_fill) ExitError("No memory for nsucc_fill", 13);

    /* Copying nsucc values to nsucc_fill and resetting nsucc to 0 for filling */
    for (i = 0; i < nnodes; ++i) {
        nsucc_fill[i] = nodes[i].nsucc;
    }

    printf("Total number of successors: %lu\n", ntotnsucc);

    /* Allocating memory for allsuccessors */
    unsigned long *allsuccessors = (unsigned long *)malloc(ntotnsucc * sizeof(unsigned long));
    if (!allsuccessors) ExitError("No memory for allsuccessors", 14);

    unsigned long *pcur = allsuccessors;
    for (unsigned long i = 0; i < nnodes; ++i) {
        nodes[i].successors = pcur;
        nodes[i].nsucc = 0;           /* reset nsucc to 0 for filling */
        pcur += nsucc_fill[i];          /* move the pointer forward by the number of successors */
    }

    /* Fourth pass is to process ways to fill successors */
    fin = fopen(csv_file, "r");
    if (!fin) ExitError("Error opening the .csv file to extract the save each node's successors", 15);

    fseek(fin, 0, SEEK_END);
    file_size = ftell(fin);
    rewind(fin);

    last_progress = -1;

    printf("Processing ways and building the graph.\n");

    while (fgets(line, MAX_LINE, fin)) {
        if (line[0] == '#') continue;
        if (strncmp(line, "way|", 4) == 0) {
            process_way(line, nodes, nnodes, nsucc_fill);
        }

        long current = ftell(fin);
        int percent = (file_size > 0) ? (int)((100.0 * current) / file_size) : 100;

        if (percent != last_progress) {
            show_progress("Building graph", current, file_size);
            last_progress = percent;
        }
    }

    fclose(fin);

    /* Write the binary file with nnodes, ntotnsucc, nodes and successors */
    FILE *bf = fopen(bin_file, "wb");
    if (!bf) ExitError("The binary file could not be opened", 16);

    /* Global data −−− header */
    if( fwrite(&nnodes, sizeof(unsigned long), 1, bf) +
        fwrite(&ntotnsucc, sizeof(unsigned long), 1, bf) != 2 )
            ExitError("when initializing the output binary data file", 17);

    /* Writing all nodes */
    if( fwrite(nodes, sizeof(node), nnodes, bf) != nnodes )
        ExitError("when writing nodes to the output binary data file", 18);

    /* Writing all successors, stored in a single vector */
    if (fwrite(allsuccessors, sizeof(unsigned long), ntotnsucc, bf) != ntotnsucc)
        ExitError("Failure when writting allsuccessors", 19);

    fclose(bf);

    printf("Binary file %s successfully generated.\n", bin_file);

    free(nodes);
    free(allsuccessors);
    free(nsucc_fill);

    return 0;
}
