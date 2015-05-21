/*
 * cluster-lexicon.c - cluster words in a lexicon according to a distance 
 *                     measure 
 * NOTE: The output is NOT sorted and will have to be sorted before use.
 */

#include "common.h"
#include <wchar.h>
#include <locale.h>
#include <langinfo.h>

typedef struct {
    int cluster1, cluster2;
    float dist;
} DISTANCE;

typedef struct {
    int size; // number of elements
    int *elements; // list of words in this cluster
    union {
        int start_offset; // position in distance array from where distances for 
                          // this cluster are stored
        int stem_len;
    };
} CLUSTER;


wchar_t *lex_buf;
int lex_buf_size;

int load_lex(char* file_name, int* lexicon, int lex_size)
{
    char inbuf[BUF_LEN/2], *temp_str;
    wchar_t *wcp, *saved;
    int i, j, l;
    long len;
    FILE * fp;

    /* determine size (in bytes) of lexicon file */
    if (NULL == (fp = fopen(file_name, "r")) ||
        UNDEF == fseek(fp, 0, SEEK_END) || 
        UNDEF == (len = ftell(fp)))
        ERR_MESG("error opening / seeking in lexicon file");
    /* assume there are on avg. two bytes per character (this should be enough 
     * for IN languages, but too little for EN)
     */
    lex_buf_size = len / 2; 
    if (NULL == (lex_buf = Malloc(lex_buf_size, wchar_t)))
        ERR_MESG("lex_buf - out of memory");

    /* read lexicon line by line, convert and store in lex_buf */
    rewind(fp);
    for (i = j = 0, wcp = lex_buf; i < lex_size; i++) {
        if (NULL == fgets(inbuf, BUF_LEN/2, fp))
            ERR_MESG("error reading lexicon file");
        inbuf[strlen(inbuf) - 1] = '\0'; // delete '\n' at end
        temp_str = &inbuf[0];
        if (wcp + BUF_LEN/2 >= lex_buf + lex_buf_size) {
#ifdef DEBUG
            fprintf(stderr, "Realloced lex_buf\n");
#endif
            saved = lex_buf;
            lex_buf_size += (lex_size - i) * 10;
            if (NULL == (lex_buf = Realloc(lex_buf, lex_buf_size, wchar_t)))
                ERR_MESG("lex_buf realloc - out of memory");
            /* reposition pointers into lex_buf */
            wcp = lex_buf + (wcp - saved);
        }
        if (UNDEF == (l = mbsrtowcs(wcp, &temp_str, BUF_LEN/2 - 1, NULL))) {
            fprintf(stderr, "Invalid multi-byte sequence at line %d\n", i+1);
            continue;
        }
        lexicon[j++] = wcp - lex_buf;
        wcp += l+1;
    }
    fclose(fp);

    return j;
}


float dist6(wchar_t *p,wchar_t *s)
{
    int n, i, j, m = 0;
    size_t l1, l2;
    float d1 = 0.0, dd1 = 1.0;

    l1 = wcslen(p);
    l2 = wcslen(s);
    n = MAX(l1, l2);

    for (i=0; i<n; i++) {
        if (p[i] != s[i]) {
            m = i;
            for(j = i; j < n; j++) {
                d1 = d1 + dd1;
                dd1 = dd1 * 0.5;
            }
            break;
        }
    }

    dd1 = (float)  m / (n - m);
    d1 = d1 / dd1;

    return d1;
}


int main(int argc, char* argv[])
{
    wchar_t *s1, *s2;
    int lex_size, *lexicon, max_distances, num_distances, i, j, k, min_index;
    int newsize, *eltp, c, c1, c2;
    float threshold, dist, min_dist, d1, d2;
    FILE *ofp, *cfp;
    DISTANCE *c_distances;
    CLUSTER *clusters;

    if (argc != 6) 
        ERR_MESG("Usage: a.out lexicon lexicon-size stem-file cluster-file threshold");

    if (0 >= (lex_size = atoi(argv[2])))
        ERR_MESG("Invalid lexicon file size");
    if (0 >= (threshold = atof(argv[5]))) 
        ERR_MESG("Invalid threshold");
    if (NULL == setlocale(LC_CTYPE, "en_US.UTF-8"))
        ERR_MESG("Couldn't change locale");
    printf("Locale set to %s\n", nl_langinfo(CODESET));

    if (NULL == (ofp = fopen(argv[3], "w")) ||
        NULL == (cfp = fopen(argv[4], "w"))) 
        ERR_MESG("error opening cluster / stem file");

    if (NULL == (lexicon = Malloc(lex_size, int)))
        ERR_MESG("lexicon - out of memory");
    if (UNDEF == (lex_size = load_lex(argv[1], lexicon, lex_size)))
        return UNDEF;

    max_distances = (unsigned) lex_size * (unsigned) lex_size / 50;
    if (NULL == (c_distances = Malloc(max_distances, DISTANCE)) ||
        NULL == (clusters = Malloc(lex_size, CLUSTER)))
        ERR_MESG("clusters / distances - out of memory");

#ifdef DEBUG
    for (i = 0; i < lex_size; i++)
        fprintf(stderr, "%d\t%ls\n", i, lex_buf + lexicon[i]);
    fprintf(stderr, "\n\n");
#endif

    /* compute pairwise distances */
    num_distances = 0;
    for (i = 0; i < lex_size; i++) {
        clusters[i].size = 1;
        clusters[i].start_offset = num_distances;
        for (j = i+1; j < lex_size; j++) {
            dist = dist6(lex_buf + lexicon[i], lex_buf + lexicon[j]);

#ifdef DEBUG
                fprintf(stderr, "Cluster %d <-> Cluster %d = %f\n", i, j, dist);
#endif

            if (dist <= threshold) {
                if (num_distances == max_distances) {
#ifdef DEBUG
                    fprintf(stderr, "Realloced distance arrays\n");
#endif
                    max_distances += (lex_size - i) * (lex_size - i) / 10 + 10;
                    if (NULL == (c_distances = Realloc(c_distances, max_distances, DISTANCE)))
                        ERR_MESG("distance array - out of memory");
                }
                c_distances[num_distances].cluster1 = i;
                c_distances[num_distances].cluster2 = j;
                c_distances[num_distances].dist = dist;
                num_distances++;
            }
	}
    }


    /* start the clustering passes */
    while(1) {
        /* find the closest pair */
        min_dist = INFTY;
        for (i = 0; i < num_distances; i++) {
            if (min_dist > c_distances[i].dist) {
                min_dist = c_distances[i].dist;
                min_index = i;
            }
        }

        if (min_dist > threshold) break; // done!

        /* merge the pair */
        c1 = c_distances[min_index].cluster1;
        c2 = c_distances[min_index].cluster2;
        c_distances[min_index].dist = INFTY; // don't consider this pair again


#ifdef DEBUG
        fprintf(stderr, "\nMerging %d and %d\nC1: ", c1, c2);
        if (clusters[c1].size == 1) 
            fprintf(stderr, "%d\nC2: ", c1);
        else {
            for (i = 0; i < clusters[c1].size; i++)
                fprintf(stderr, "%d ", clusters[c1].elements[i]);
            fprintf(stderr, "\nC2: ");
        }
        if (clusters[c2].size == 1) 
            fprintf(stderr, "%d\n", c2);
        else {
            for (i = 0; i < clusters[c2].size; i++)
                fprintf(stderr, "%d ", clusters[c2].elements[i]);
            fprintf(stderr, "\n");
        }
#endif

        newsize = clusters[c1].size + clusters[c2].size;
        if (NULL == (eltp = Malloc(newsize, int)))
            ERR_MESG("merging clusters - out of memory");
        if (clusters[c1].size == 1) /* only a single element => it contains the "c1"-eth word */
            eltp[0] = c1;
        else {
            for (i = 0; i < clusters[c1].size; i++)
                eltp[i] = clusters[c1].elements[i];
            free(clusters[c1].elements);
        }
        if (clusters[c2].size == 1) /* only a single element => it contains the "c2"-eth word */
            eltp[clusters[c1].size] = c2;
        else {
            for (i = 0; i < clusters[c2].size; i++)
                eltp[clusters[c1].size + i] = clusters[c2].elements[i];
            free(clusters[c2].elements);
        }
        clusters[c1].size = newsize;
        clusters[c1].elements = eltp;
        /* c2 no longer exists */
        clusters[c2].size = 0;

        /* update similarities */
        /* for any cluster c, dist(c1+c2, c) = MAX(dist(c1, c), dist(c2, c)) */ 
        /* NB: c1 < c2 */
        /* clusters before c1 */
        for (i = 0; i < c1; i++) {
            if (clusters[i].size > 0) { // this is a valid cluster
                j = clusters[i].start_offset;
                d1 = INFTY; k = UNDEF;
                // look for distance with c1
                while (j < clusters[i+1].start_offset) {
                    if (c_distances[j].cluster2 == c1) {
                        k = j;
                        d1 = c_distances[j].dist;
                        break;
                    }
                    j++;
                }
                // now look for distance with c2
                if (j < clusters[i+1].start_offset) {
                    d2 = INFTY;
                    while (j < clusters[i+1].start_offset) {
                        if (c_distances[j].cluster2 == c2) {
                            d2 = c_distances[j].dist;
                            c_distances[j].dist = INFTY;
                            break;
                        }
                        j++;
                    }
                    c_distances[k].dist = MAX(d1, d2);
                }
                else {
                    /* this cluster is too far from c1 (and therefore from c1+c2) */
                    /* only remains to invalidate the distance with c2, if any */
                    for (j = clusters[i].start_offset; j < clusters[i+1].start_offset; j++) {
                        if (c_distances[j].cluster2 == c2) {
                            c_distances[j].dist = INFTY;
                            break;
                        }
                    }
                }
            } // end valid cluster
        }
        /* cluster c1 */
        for (i = clusters[c1].start_offset; i < clusters[c1+1].start_offset; i++) {
            if (c_distances[i].dist == INFTY) continue;
            c = c_distances[i].cluster2;
            /* find dist(c, c2) */
            d1 = INFTY;
            if (c < c2) {
                for (j = clusters[c].start_offset; j < clusters[c+1].start_offset; j++)
                    if (c_distances[j].cluster2 == c2) {
                        d1 = c_distances[j].dist;
                        break;
                    }
            }
            else { // c > c2
                for (j = clusters[c2].start_offset; j < clusters[c2+1].start_offset; j++)
                    if (c_distances[j].cluster2 == c) {
                        d1 = c_distances[j].dist; 
                        break;
                    }
            }
            if (d1 > c_distances[i].dist) c_distances[i].dist = d1;
        }
        /* clusters between c1 and c2 */
        for (i = c1+1; i < c2; i++) {
            if (clusters[i].size > 0) { // this is a valid cluster
                for (j = clusters[i].start_offset; j < clusters[i+1].start_offset; j++) {
                    if (c_distances[j].cluster2 == c2) {
                        c_distances[j].dist = INFTY;
                        break;
                    }
                }
            }
        }
        /* cluster c2 */
        if (c2 != lex_size - 1) // if c2 is the last cluster, nothing to do 
            for (j = clusters[c2].start_offset; j < clusters[c2+1].start_offset; j++)
                c_distances[j].dist = INFTY;
        /* clusters after c2 need not be considered */

#ifdef DEBUG
        fprintf(stderr, "Updated distances:\n");
        for (i = 0; i < num_distances; i++) 
            if (c_distances[i].dist != INFTY) 
                fprintf(stderr, "Cluster %d <-> Cluster %d = %f\n", 
                        c_distances[i].cluster1, c_distances[i].cluster2, 
                        c_distances[i].dist);
        fprintf(stderr, "\n");
#endif

    } /* end clustering passes */


    /* find the stems for each cluster (just remember the stem length) */
    /* also write out each cluster and the stems */
    for (i = 0; i < lex_size; i++) {
        if (clusters[i].size == 0) continue; // invalid cluster
        if (clusters[i].size == 1) { // singleton cluster
            fprintf(cfp, "%ls\n", lex_buf + lexicon[i]);
            fprintf(ofp, "%ls %ls\n", lex_buf + lexicon[i], lex_buf + lexicon[i]);
            continue;
        }
        s1 = lex_buf + lexicon[clusters[i].elements[0]];
        clusters[i].stem_len = wcslen(s1);
        fprintf(cfp, "%ls", s1);
        /* find the longest common prefix */
        for (j = 1; j < clusters[i].size; j++) {
            s2 = lex_buf + lexicon[clusters[i].elements[j]];
            fprintf(cfp, " %ls", s2);
            for (k = 0; s1[k] == s2[k] && k < clusters[i].stem_len; k++);
            if (k < clusters[i].stem_len)
                clusters[i].stem_len = k;
        }
        fprintf(cfp, "\n");
        fprintf(ofp, "%ls\t", s1);
        s1[clusters[i].stem_len] = (wchar_t) 0;
        fprintf(ofp, "%ls\n", s1);
        for (j = 1; j < clusters[i].size; j++) {
            s2 = lex_buf + lexicon[clusters[i].elements[j]];
            fprintf(ofp, "%ls\t%ls\n", s2, s1);
        }
    }


    /* clean up */
    for (i = 0; i < lex_size; i++)
        if (clusters[i].size > 1) free(clusters[i].elements);
    free(lex_buf); free(lexicon); free(c_distances); free(clusters);
    fclose(ofp); fclose(cfp);
    return 1;
}
