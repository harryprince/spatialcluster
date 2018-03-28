#include "common.h"
#include "utils.h"

// Note that all matrices **CAN** be asymmetrical, and so are always indexed
// (from, to)

unsigned int sets_init (
        const Rcpp::IntegerVector &from,
        const Rcpp::IntegerVector &to,
        uint_map_t &vert2index_map,
        uint_map_t &index2vert_map,
        uint_map_t &vert2cl_map,
        uint_set_map_t &cl2vert_map)
{
    vert2index_map.clear ();
    vert2cl_map.clear ();
    cl2vert_map.clear ();

    std::unordered_set <unsigned int> vert_set;
    for (int i = 0; i < from.size (); i++)
    {
        vert_set.emplace (from [i]);
        vert_set.emplace (to [i]);
    }
    unsigned int i = 0;
    for (auto v: vert_set)
    {
        index2vert_map.emplace (i, v);
        vert2index_map.emplace (v, i++);
    }

    for (int i = 0; i < from.length (); i++)
    {
        std::set <unsigned int> eset;
        unsigned int fi = vert2index_map.at (from [i]);
        eset.insert (fi);
        cl2vert_map.emplace (fi, eset);
    }
    for (int i = 0; i < to.length (); i++)
    {
        unsigned int ti = vert2index_map.at (to [i]);
        if (cl2vert_map.find (ti) == cl2vert_map.end ())
        {
            std::set <unsigned int> eset;
            eset.insert (ti);
            cl2vert_map.emplace (ti, eset);
        } else
        {
            std::set <unsigned int> eset = cl2vert_map.at (ti);
            eset.emplace (ti);
            cl2vert_map.at (ti) = eset;
        }
    }
    
    const unsigned int n = vert_set.size ();
    // Initially assign all verts to clusters of same number:
    for (unsigned int i = 0; i < n; i++)
        vert2cl_map.emplace (i, i);

    return n;
}

//' initial contiguity and distance matrices. The contiguity matrix is between
//' clusters, so is constantly modified, whereas the distance matrix is between
//' edges, so is fixed at load time.
//' @noRd
void mats_init (
        const Rcpp::IntegerVector &from,
        const Rcpp::IntegerVector &to,
        const Rcpp::NumericVector &d,
        uint_map_t &vert2index_map,
        arma::Mat <unsigned short> &contig_mat,
        arma::Mat <double> &d_mat)
{
    const unsigned int n = vert2index_map.size ();

    contig_mat = arma::zeros <arma::Mat <unsigned short> > (n, n);
    //d_mat = arma::zeros <arma::Mat <double> > (n, n);
    d_mat.resize (n, n);
    d_mat.fill (INFINITE_DOUBLE);

    for (int i = 0; i < from.length (); i++)
    {
        contig_mat (vert2index_map.at (from [i]),
                vert2index_map.at (to [i])) = 1;
        d_mat (vert2index_map.at (from [i]),
            vert2index_map.at (to [i])) = d [i];
    }
}

void dmat_full_init (
        const Rcpp::IntegerVector &from, // here, from_full, etc.
        const Rcpp::IntegerVector &to,
        const Rcpp::NumericVector &d,
        uint_map_t &vert2index_map,
        arma::Mat <double> &d_mat) // here, d_mat_full
{
    //d_mat = arma::zeros <arma::Mat <double> > (n, n);
    d_mat.resize (vert2index_map.size (), vert2index_map.size ());
    d_mat.fill (INFINITE_DOUBLE);

    for (int i = 0; i < from.length (); i++)
    {
        d_mat [vert2index_map.at (from [i]),
              vert2index_map.at (to [i])] = d [i];
    }
}

//' find shortest connection between two clusters
//' @param from, to, d the columns of the edge graph
//' @param d_mat distance matrix between all edges (not between clusters!)
//' @param cl2vert_map map of list of all (from, to, d) edges for each cluster
//' @param cfrom Number of cluster which is to be merged
//' @param cto Number of cluster with which it is to be merged
//'
//' @return Index directly into from, to - **NOT** into the actual matrices!
//' @noRd
int find_shortest_connection (
        Rcpp::IntegerVector &from,
        Rcpp::IntegerVector &to,
        Rcpp::NumericVector &d,
        uint_map_t &vert2index_map,
        arma::Mat <double> &d_mat,
        uint_set_map_t &cl2vert_map,
        int cfrom,
        int cto)
{
    std::set <unsigned int> verts_i = cl2vert_map.at (cfrom);
    std::set <unsigned int> verts_j = cl2vert_map.at (cto);

    double dmin = INFINITE_DOUBLE;
    int short_i = INFINITE_INT, short_j = INFINITE_INT;

    // from and to here are not direction, so need to examine both directions
    for (auto i: verts_i)
        for (auto j: verts_j)
        {
            if (d_mat (i, j) < dmin)
            {
                dmin = d_mat (i, j);
                short_i = i;
                short_j = j;
            } else if (d_mat (j, i) < dmin)
            {
                dmin = d_mat (j, i);
                short_i = j;
                short_j = i;
            }
        }
    if (dmin == INFINITE_DOUBLE)
        Rcpp::stop ("no minimal distance; this should not happen");

    // convert short_i and short_j to a single edge 
    // TODO: Make a std::map of vert2dist to avoid this loop
    int shortest = INFINITE_INT;
    for (int i = 0; i < from.length (); i++)
    {
        if (vert2index_map.at (from [i]) == short_i &&
                vert2index_map.at (to [i]) == short_j)
        {
            shortest = i;
            break;
        }
    }
    if (shortest == INFINITE_INT)
        Rcpp::stop ("shite");

    return shortest;
}

//' merge two clusters in the contiguity matrix, reducing the size of the matrix
//' by one row and column.
//' @noRd
void merge_clusters (
        arma::Mat <unsigned short> &contig_mat,
        uint_map_t &vert2cl_map,
        uint_set_map_t &cl2vert_map,
        int cluster_from,
        int cluster_to)
{
    // Set all contig_mat (cluster_from, .) to 1
    for (unsigned int j = 0; j < contig_mat.n_rows; j++)
    {
        if (contig_mat (cluster_from, j) == 1 )
        {
            contig_mat (cluster_to, j) = 1;
            contig_mat (j, cluster_to) = 1;
        }
    }

    std::set <unsigned int> verts_from = cl2vert_map.at (cluster_from),
        verts_to = cl2vert_map.at (cluster_to);

    for (auto vi: verts_from)
        for (auto vj: verts_to)
        {
            // not directonal here, so need both directions:
            contig_mat (vi, vj) = contig_mat (vj, vi) = 1;
        }

    // then re-number all cluster numbers in cl2vert 
    cl2vert_map.erase (cluster_from);
    for (auto v: verts_from)
        verts_to.insert (v);
    cl2vert_map.at (cluster_to) = verts_to;
    // and in vert2cl:
    for (auto v: verts_from)
        vert2cl_map [v] = cluster_to;
}
