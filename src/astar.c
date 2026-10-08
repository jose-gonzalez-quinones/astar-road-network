#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct {
    unsigned long id; 
    char *name;         
    double lat, lon;  
    unsigned short nsucc;
    unsigned long *successors; 
} node;

#define INF 1e30

static void ExitError(const char *msg, int errcode) {
    fprintf(stderr, "Error: %s\n", msg);
    exit(errcode);
}

/* Use binary search. */
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

/* Geodesic distance (Haversine) in meters */
static double geo_distance(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371000.0;  /* Earth radius in meters */
    const double deg2rad = M_PI / 180.0;

    double phi1 = lat1 * deg2rad;
    double phi2 = lat2 * deg2rad;
    double dphi = (lat2 - lat1) * deg2rad;
    double dlambda = (lon2 - lon1) * deg2rad;

    double a = sin(dphi/2.0)*sin(dphi/2.0) +
               cos(phi1)*cos(phi2)*sin(dlambda/2.0)*sin(dlambda/2.0);
    double c = 2.0 * atan2(sqrt(a), sqrt(1.0-a));

    return R * c;
}

/* Priority queue for A* */

typedef struct {
    unsigned long node_idx;
    double fscore;
} HeapNode;

typedef struct {
    HeapNode *data;
    unsigned long size;
    unsigned long capacity;
} MinHeap;

static MinHeap *heap_create(unsigned long cap) {
    MinHeap *h = (MinHeap *)malloc(sizeof(MinHeap));
    if (!h) ExitError("There is no memory for MinHeap", 20);
    h->data = (HeapNode *)malloc(cap * sizeof(HeapNode));
    if (!h->data) ExitError("There is no memory for MinHeap data.", 21);
    h->size = 0;
    h->capacity = cap;
    return h;
}

static void heap_swap(HeapNode *a, HeapNode *b) {
    HeapNode tmp = *a;
    *a = *b;
    *b = tmp;
}

static void heap_push(MinHeap *h, unsigned long node_idx, double fscore) {
    if (h->size == h->capacity) {
        h->capacity *= 2;
        h->data = (HeapNode *)realloc(h->data, h->capacity * sizeof(HeapNode));
        if (!h->data) ExitError("no memory when resizing heap", 22);
    }
    unsigned long i = h->size++;
    h->data[i].node_idx = node_idx;
    h->data[i].fscore = fscore;

    /* upward move */
    while (i > 0) {
        unsigned long parent = (i - 1) / 2;
        if (h->data[parent].fscore <= h->data[i].fscore) break;
        heap_swap(&h->data[parent], &h->data[i]);
        i = parent;
    }
}

static int heap_empty(MinHeap *h) {
    return h->size == 0;
}

static HeapNode heap_pop(MinHeap *h) {
    if (h->size == 0) ExitError("empty heap pop", 23);
    HeapNode root = h->data[0];
    h->size--;
    if (h->size > 0) {
        h->data[0] = h->data[h->size];
        /* downward move */
        unsigned long i = 0;
        while (1) {
            unsigned long left = 2*i + 1;
            unsigned long right = 2*i + 2;
            unsigned long smallest = i;

            if (left < h->size && h->data[left].fscore < h->data[smallest].fscore)
                smallest = left;
            if (right < h->size && h->data[right].fscore < h->data[smallest].fscore)
                smallest = right;

            if (smallest == i) break;
            heap_swap(&h->data[i], &h->data[smallest]);
            i = smallest;
        }
    }
    return root;
}

static void heap_free(MinHeap *h) {
    if (!h) return;
    free(h->data);
    free(h);
}

/* A* function */
static int run_astar(node *nodes,
                     unsigned long nnodes,
                     unsigned long start_id,
                     unsigned long goal_id,
                     const char *out_file)
{
    /* Locate the indexes of source and target */
    long start_idx = find_node_index(nodes, nnodes, start_id);
    long goal_idx  = find_node_index(nodes, nnodes, goal_id);
    if (start_idx < 0 || goal_idx < 0)
        ExitError("start_id or goal_id not found in the graph",24);

    printf("Start idx = %ld, goal idx = %ld\n", start_idx, goal_idx);

    /* Allocate arrays for A* */
    double *gscore = (double *)malloc(nnodes * sizeof(double));
    double *fscore = (double *)malloc(nnodes * sizeof(double));
    int *closed    = (int *)calloc(nnodes, sizeof(int));
    long *parent   = (long *)malloc(nnodes * sizeof(long));
    if (!gscore || !fscore || !closed || !parent)
        ExitError("There is no memory for A* structures", 25);

    for (unsigned long i = 0; i < nnodes; ++i) {
        gscore[i] = INF;
        fscore[i] = INF;
        parent[i] = -1;
    }

    MinHeap *open_set = heap_create(1024);

    gscore[start_idx] = 0.0;
    fscore[start_idx] = geo_distance(nodes[start_idx].lat, nodes[start_idx].lon,
                                     nodes[goal_idx].lat,  nodes[goal_idx].lon);
    heap_push(open_set, (unsigned long)start_idx, fscore[start_idx]);

    unsigned long expanded = 0;
    int found = 0;

    /* Main A* loop */
    while (!heap_empty(open_set)) {
        HeapNode current = heap_pop(open_set);
        unsigned long u = current.node_idx;

        if (closed[u]) continue;   /* skip obsolete entries */

        closed[u] = 1;
        expanded++;

        if ((long)u == goal_idx) {
            found = 1;
            break;
        }

        node *nu = &nodes[u];

        for (unsigned short k = 0; k < nu->nsucc; ++k) {
            unsigned long v = nu->successors[k];
            if (closed[v]) continue;

            node *nv = &nodes[v];
            double cost_uv = geo_distance(nu->lat, nu->lon, nv->lat, nv->lon);
            double tentative_g = gscore[u] + cost_uv;

            if (tentative_g < gscore[v]) {
                parent[v] = (long)u;
                gscore[v] = tentative_g;
                double h = geo_distance(nv->lat, nv->lon,
                                        nodes[goal_idx].lat, nodes[goal_idx].lon);
                fscore[v] = gscore[v] + h;
                heap_push(open_set, v, fscore[v]);
            }
        }
    }

    if (!found) {
        fprintf(stderr, "No path found between %lu and %lu.\n", start_id, goal_id);
        heap_free(open_set);
        free(gscore);
        free(fscore);
        free(closed);
        free(parent);
        return 1;
    }

    /* Reconstruct the path */
    unsigned long path_len = 0;
    long cur = goal_idx;
    while (cur != -1) {
        path_len++;
        cur = parent[cur];
    }

    unsigned long *path = (unsigned long *)malloc(path_len * sizeof(unsigned long));
    if (!path) ExitError("no memory for path", 26);

    cur = goal_idx;
    for (long i = (long)path_len - 1; i >= 0; --i) {
        path[i] = (unsigned long)cur;
        cur = parent[cur];
    }

    /* Write the output file */
    FILE *fout = fopen(out_file, "w");
    if (!fout) ExitError("The output file cannot be opened", 27);

    fprintf(fout, "# We have expanded %lu nodes \n", expanded);
    fprintf(fout, "# Distance from %lu to %lu: %.6f metres.\n",
            start_id, goal_id, gscore[goal_idx]);
    fprintf(fout, "# Optimal path:\n");

    for (unsigned long i = 0; i < path_len; ++i) {
        unsigned long idxn = path[i];
        node *nd = &nodes[idxn];
        fprintf(fout, "Id = %lu | %.6f | %.6f | Dist = %.6f\n",
                nd->id, nd->lat, nd->lon, gscore[idxn]);
    }

    fclose(fout);

    printf("Optimal path written in %s (expanded nodes: %lu, distance: %.3f m)\n",
           out_file, expanded, gscore[goal_idx]);

    /* Local cleanup */
    heap_free(open_set);
    free(gscore);
    free(fscore);
    free(closed);
    free(parent);
    free(path);

    return 0;
}

/* Read graph and call run_astar */

int main(int argc, char *argv[]) {
    const char *bin_file = "data/spain.bin";
    const char *out_file = "examples/route.txt";

    unsigned long start_id = 240949599UL;  /* Santa Maria del Mar (Barcelona) */
    unsigned long goal_id  = 195977239UL;  /* La Giralda (Seville) */

    if (argc > 1) bin_file = argv[1];
    if (argc > 2) start_id = strtoul(argv[2], NULL, 10);
    if (argc > 3) goal_id  = strtoul(argv[3], NULL, 10);
    if (argc > 4) out_file = argv[4];

    /* Read the binary graph file */
    FILE *fin = fopen(bin_file, "rb");
    if (!fin) ExitError("cannot open the binary graph file", 28);
    
    unsigned long nnodes, ntotnsucc;
    if (fread(&nnodes, sizeof(unsigned long), 1, fin) != 1 ||
        fread(&ntotnsucc, sizeof(unsigned long), 1, fin) != 1)
        ExitError("error reading binary header", 29);

    node *nodes = (node *)malloc(nnodes * sizeof(node));
    if (!nodes) ExitError("There is no memory for nodes", 30);

    unsigned long *allsuccessors = (unsigned long *)malloc(ntotnsucc * sizeof(unsigned long));
    if (!allsuccessors) ExitError("There is no memory for allsuccessors", 31);

    if (fread(nodes, sizeof(node), nnodes, fin) != nnodes)
        ExitError("Error reading nodes from binary", 32);
    if (fread(allsuccessors, sizeof(unsigned long), ntotnsucc, fin) != ntotnsucc)
        ExitError("Error reading allsuccessors from binary", 33);

    fclose(fin);

    /* Rebuild successors pointers */
    unsigned long *pcur = allsuccessors;
    for (unsigned long i = 0; i < nnodes; ++i) {
        if (nodes[i].nsucc > 0) {
            nodes[i].successors = pcur;
            pcur += nodes[i].nsucc;
        } else {
            nodes[i].successors = NULL;
        }
        nodes[i].name = NULL;
    }

    int status = run_astar(nodes, nnodes, start_id, goal_id, out_file);

    free(nodes);
    free(allsuccessors);

    return status;
}
