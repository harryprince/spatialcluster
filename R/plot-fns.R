#' scl_hulls
#'
#' Calculate convex hulls around clusters, mostly cribbed from
#' osmplotr/R/add-osm-groups.R
#'
#' @param tree Spanning tree obtained from \link{scl_cluster}
#' @param xy Matrix of spatial coordinates of points indexed by \code{tree}.
#' @return tibble of (id, x, y), where the coordinates trace the convex hulls
#' for each cluster id
#' @noRd
scl_hulls <- function (tree, xy)
{
    xy <- as.matrix (xy)
    ncomp <- length (unique (tree$comp))
    bdry <- list ()
    for (i in seq (ncomp))
    {
        if (length (which (tree$comp == i)) > 1)
        {
            xyi <- tree %>%
                dplyr::filter (comp == i) %>%
                dplyr::select (from, to) %>%
                unlist () %>%
                unique () %>%
                sort () %>%
                xy [., ]
            xy2 <- spatstat::ppp (xyi [, 1], xyi [, 2],
                                  xrange = range (xyi [, 1]),
                                  yrange = range (xyi [, 2]))
            ch <- spatstat::convexhull (xy2)
            bdry [[i]] <- cbind (i, ch$bdry[[1]]$x, ch$bdry[[1]]$y)
        }
    }
    bdry <- data.frame (do.call (rbind, bdry))
    names (bdry) <- c ("id", "x", "y")
    return (bdry)
}

#' plot.scl
#' @method plot scl
#' @param x object to be plotted
#' @param ... ignored here
#' @export
#' @examples
#' n <- 20
#' xy <- matrix (runif (2 * n), ncol = 2)
#' dmat <- matrix (runif (n ^ 2), ncol = n)
#' scl <- scl_cluster (xy, dmat, ncl = 4, shortest = TRUE)
#' plot (scl)
#' # Connect clusters according to highest (\code{shortest = FALSE}) values of
#' # \coce{dmat}:
#' scl <- scl_cluster (xy, dmat, ncl = 4, shortest = FALSE)
#' plot (scl)
plot.scl <- function (x, ...)
{
    hulls <- scl_hulls (x$tree, x$xy)
    nc <- length (unique (x$tree$comp))

    cl_cols <- rainbow (nc) %>%
        tibble::as.tibble () %>%
        dplyr::mutate (comp = seq (nc)) %>%
        dplyr::rename (col = value)

    tree <- dplyr::left_join (x$tree, cl_cols, by = "comp")
    edge2vert <- dplyr::bind_rows (dplyr::select (x$tree, c (from, comp)) %>%
                                       dplyr::rename (v = from),
                                   dplyr::select (x$tree, c (to, comp)) %>%
                                       dplyr::rename (v = to)) %>%
                dplyr::arrange (v) %>%
                unique ()
    xy <- x$xy
    xy %<>% dplyr::mutate (v = seq (nrow (xy))) %>%
        dplyr::left_join (edge2vert, by = "v") %>%
        dplyr::mutate (comp = ifelse (is.na (comp), 1, comp + 1)) %>%
        dplyr::left_join (cl_cols, by = "comp") %>%
        dplyr::mutate (col = ifelse (is.na (col), "#222222", col))

    hull_aes <- ggplot2::aes (x = x, y = y, group = id)
    hull_width <- 0.5
    g <- ggplot2::ggplot (xy, ggplot2::aes (x = x,
                                            y = y,
                                            colour = col)) +
        ggplot2::geom_point (size = 5, show.legend = FALSE) +
        ggplot2::geom_polygon (data = hulls,
                               mapping = hull_aes,
                               colour = cl_cols$col [hulls$id],
                               fill = cl_cols$col [hulls$id],
                               alpha = 0.1,
                               size = hull_width) +
        ggthemes::theme_solarized ()
    print (g)
    invisible (g)
}