#ifdef __unix__
#define _POSIX_C_SOURCE 200112L
#endif

/* ######################### includes ############################################################################### */

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>

#include "dump.h"
#include "event_int.h"
#include "gks.h"
#include "gr.h"
#include "logging_int.h"
#include "plot_int.h"

#include "datatype/string_map_int.h"
#include "datatype/template/map_int.h"
#include "datatype/template/set_int.h"


/* ######################### private interface ###################################################################### */

/* ========================= datatypes ============================================================================== */

/* ------------------------- args set ------------------------------------------------------------------------------- */

DECLARE_SET_TYPE(args, grm_args_t *)


/* ------------------------- string-to-plot_func map ---------------------------------------------------------------- */

DECLARE_MAP_TYPE(plot_func, plot_func_t)


/* ------------------------- string-to-args_set map ----------------------------------------------------------------- */

DECLARE_MAP_TYPE(args_set, args_set_t *)


#undef DECLARE_SET_TYPE
#undef DECLARE_MAP_TYPE


/* ========================= macros ================================================================================= */

/* ------------------------- math ----------------------------------------------------------------------------------- */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


/* ========================= methods ================================================================================ */

/* ------------------------- args set ------------------------------------------------------------------------------- */

DECLARE_SET_METHODS(args)


/* ------------------------- string-to-plot_func map ---------------------------------------------------------------- */

DECLARE_MAP_METHODS(plot_func)


/* ------------------------- string-to-args_set map ----------------------------------------------------------------- */

DECLARE_MAP_METHODS(args_set)


#undef DECLARE_SET_METHODS
#undef DECLARE_MAP_METHODS


/* ######################### internal implementation ################################################################ */

/* ========================= static variables ======================================================================= */

/* ------------------------- plot ----------------------------------------------------------------------------------- */

/* ~~~~~~~~~~~~~~~~~~~~~~~~~ general ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

static int plot_static_variables_initialized = 0;
const char *plot_hierarchy_names[] = {"root", "plots", "subplots", "series", NULL};
static int plot_scatter_markertypes[] = {
    GKS_K_MARKERTYPE_SOLID_CIRCLE,   GKS_K_MARKERTYPE_SOLID_TRI_UP, GKS_K_MARKERTYPE_SOLID_TRI_DOWN,
    GKS_K_MARKERTYPE_SOLID_SQUARE,   GKS_K_MARKERTYPE_SOLID_BOWTIE, GKS_K_MARKERTYPE_SOLID_HGLASS,
    GKS_K_MARKERTYPE_SOLID_DIAMOND,  GKS_K_MARKERTYPE_SOLID_STAR,   GKS_K_MARKERTYPE_SOLID_TRI_RIGHT,
    GKS_K_MARKERTYPE_SOLID_TRI_LEFT, GKS_K_MARKERTYPE_SOLID_PLUS,   GKS_K_MARKERTYPE_PENTAGON,
    GKS_K_MARKERTYPE_HEXAGON,        GKS_K_MARKERTYPE_HEPTAGON,     GKS_K_MARKERTYPE_OCTAGON,
    GKS_K_MARKERTYPE_STAR_4,         GKS_K_MARKERTYPE_STAR_5,       GKS_K_MARKERTYPE_STAR_6,
    GKS_K_MARKERTYPE_STAR_7,         GKS_K_MARKERTYPE_STAR_8,       GKS_K_MARKERTYPE_VLINE,
    GKS_K_MARKERTYPE_HLINE,          GKS_K_MARKERTYPE_OMARK,        INT_MAX};


/* ~~~~~~~~~~~~~~~~~~~~~~~~~ args ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

static grm_args_t *global_root_args = NULL;
grm_args_t *active_plot_args = NULL;
static unsigned int active_plot_index = 0;

/* ~~~~~~~~~~~~~~~~~~~~~~~~~ event handling ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

event_queue_t *event_queue = NULL;


/* ~~~~~~~~~~~~~~~~~~~~~~~~~ kind to fmt ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* TODO: Check format of: "hist", "isosurface", "imshow"  */
static string_map_entry_t kind_to_fmt[] = {{"line", "xys"},      {"hexbin", "xys"},
                                           {"polar", "xys"},     {"shade", "xys"},
                                           {"stem", "xys"},      {"step", "xys"},
                                           {"contour", "xyzc"},  {"contourf", "xyzc"},
                                           {"tricont", "xyzc"},  {"trisurf", "xyzc"},
                                           {"surface", "xyzc"},  {"wireframe", "xyzc"},
                                           {"plot3", "xyzc"},    {"scatter", "xyzc"},
                                           {"scatter3", "xyzc"}, {"quiver", "xyuv"},
                                           {"heatmap", "xyzc"},  {"hist", "x"},
                                           {"barplot", "xy"},    {"isosurface", "x"},
                                           {"imshow", ""},       {"nonuniformheatmap", "xyzc"}};


/* ~~~~~~~~~~~~~~~~~~~~~~~~~ kind to func ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

static plot_func_map_entry_t kind_to_func[] = {
    {"line", plot_line},
    {"step", plot_step},
    {"scatter", plot_scatter},
    {"quiver", plot_quiver},
    {"stem", plot_stem},
    {"hist", plot_hist},
    {"barplot", plot_barplot},
    {"contour", plot_contour},
    {"contourf", plot_contourf},
    {"hexbin", plot_hexbin},
    {"heatmap", plot_heatmap},
    {"wireframe", plot_wireframe},
    {"surface", plot_surface},
    {"plot3", plot_plot3},
    {"scatter3", plot_scatter3},
    {"imshow", plot_imshow},
    {"isosurface", plot_isosurface},
    {"polar", plot_polar},
    {"trisurf", plot_trisurf},
    {"tricont", plot_tricont},
    {"shade", plot_shade},
    {"nonuniformheatmap", plot_heatmap},
};


/* ~~~~~~~~~~~~~~~~~~~~~~~~~ maps ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

static string_map_t *fmt_map = NULL;
static plot_func_map_t *plot_func_map = NULL;
static string_map_t *plot_valid_keys_map = NULL;


/* ~~~~~~~~~~~~~~~~~~~~~~~~~ plot clear ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

const char *plot_clear_exclude_keys[] = {"array_index", "in_use", NULL};


/* ~~~~~~~~~~~~~~~~~~~~~~~~~ plot merge ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

const char *plot_merge_ignore_keys[] = {"id", "series_id", "subplot_id", "plot_id", "array_index", "in_use", NULL};
const char *plot_merge_clear_keys[] = {"series", NULL};


/* ~~~~~~~~~~~~~~~~~~~~~~~~~ valid keys ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* IMPORTANT: Every key should only be part of ONE array -> keys can be assigned to the right object, if a user sends a
 * flat object that mixes keys of different hierarchies */

const char *valid_root_keys[] = {"plots", "append_plots", "hold_plots", NULL};
const char *valid_plot_keys[] = {"clear", "figsize", "size", "subplots", "update", NULL};
const char *valid_subplot_keys[] = {"adjust_xlim",  "adjust_ylim",
                                    "adjust_zlim",  "backgroundcolor",
                                    "colormap",     "keep_aspect_ratio",
                                    "kind",         "labels",
                                    "levels",       "location",
                                    "nbins",        "panzoom",
                                    "reset_ranges", "rotation",
                                    "series",       "subplot",
                                    "tilt",         "title",
                                    "xbins",        "xflip",
                                    "xform",        "xlabel",
                                    "xlim",         "xlog",
                                    "ybins",        "yflip",
                                    "ylabel",       "ylim",
                                    "ylog",         "zflip",
                                    "zlim",         "zlog",
                                    "clim",         NULL};
const char *valid_series_keys[] = {"a", "c", "markertype", "s", "spec", "step_where", "u", "v", "x", "y", "z", NULL};

/* ========================= functions ============================================================================== */

/* ------------------------- plot ----------------------------------------------------------------------------------- */

/* ~~~~~~~~~~~~~~~~~~~~~~~~~ general ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

error_t plot_init_static_variables(void)
{
  error_t error = NO_ERROR;

  if (!plot_static_variables_initialized)
    {
      logger((stderr, "Initializing static plot variables\n"));
      event_queue = event_queue_new();
      global_root_args = grm_args_new();
      error_cleanup_and_set_error_if(global_root_args == NULL, ERROR_MALLOC);
      error = plot_init_args_structure(global_root_args, plot_hierarchy_names, 1);
      error_cleanup_if_error;
      plot_set_flag_defaults();
      error_cleanup_and_set_error_if(!args_values(global_root_args, "plots", "a", &active_plot_args), ERROR_INTERNAL);
      active_plot_index = 1;
      fmt_map = string_map_new_with_data(array_size(kind_to_fmt), kind_to_fmt);
      error_cleanup_and_set_error_if(fmt_map == NULL, ERROR_MALLOC);
      plot_func_map = plot_func_map_new_with_data(array_size(kind_to_func), kind_to_func);
      error_cleanup_and_set_error_if(plot_func_map == NULL, ERROR_MALLOC);
      {
        const char **hierarchy_keys[] = {valid_root_keys, valid_plot_keys, valid_subplot_keys, valid_series_keys, NULL};
        const char **hierarchy_names_ptr, ***hierarchy_keys_ptr, **current_key_ptr;
        plot_valid_keys_map = string_map_new(array_size(valid_root_keys) + array_size(valid_plot_keys) +
                                             array_size(valid_subplot_keys) + array_size(valid_series_keys));
        error_cleanup_and_set_error_if(plot_valid_keys_map == NULL, ERROR_MALLOC);
        hierarchy_keys_ptr = hierarchy_keys;
        hierarchy_names_ptr = plot_hierarchy_names;
        while (*hierarchy_names_ptr != NULL && *hierarchy_keys_ptr != NULL)
          {
            current_key_ptr = *hierarchy_keys_ptr;
            while (*current_key_ptr != NULL)
              {
                string_map_insert(plot_valid_keys_map, *current_key_ptr, *hierarchy_names_ptr);
                ++current_key_ptr;
              }
            ++hierarchy_names_ptr;
            ++hierarchy_keys_ptr;
          }
      }
      plot_static_variables_initialized = 1;
    }
  return NO_ERROR;

error_cleanup:
  if (global_root_args != NULL)
    {
      grm_args_delete(global_root_args);
      global_root_args = NULL;
    }
  if (fmt_map != NULL)
    {
      string_map_delete(fmt_map);
      fmt_map = NULL;
    }
  if (plot_func_map != NULL)
    {
      plot_func_map_delete(plot_func_map);
      plot_func_map = NULL;
    }
  if (plot_valid_keys_map != NULL)
    {
      string_map_delete(plot_valid_keys_map);
      plot_valid_keys_map = NULL;
    }
  return error;
}


/* ~~~~~~~~~~~~~~~~~~~~~~~~~ plot arguments ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

error_t plot_merge_args(grm_args_t *args, const grm_args_t *merge_args, const char **hierarchy_name_ptr,
                        uint_map_t *hierarchy_to_id, int hold_always)
{
  static args_set_map_t *key_to_cleared_args = NULL;
  static int recursion_level = -1;
  int plot_id, subplot_id, series_id;
  int append_plots;
  args_iterator_t *merge_it = NULL;
  arg_t *arg, *merge_arg;
  args_value_iterator_t *value_it = NULL, *merge_value_it = NULL;
  const char **current_hierarchy_name_ptr;
  grm_args_t **args_array, **merge_args_array, *current_args;
  unsigned int i;
  error_t error = NO_ERROR;

  ++recursion_level;
  if (hierarchy_name_ptr == NULL)
    {
      hierarchy_name_ptr = plot_hierarchy_names;
    }
  if (hierarchy_to_id == NULL)
    {
      hierarchy_to_id = uint_map_new(array_size(plot_hierarchy_names));
      cleanup_and_set_error_if(hierarchy_to_id == NULL, ERROR_MALLOC);
    }
  else
    {
      hierarchy_to_id = uint_map_copy(hierarchy_to_id);
      cleanup_and_set_error_if(hierarchy_to_id == NULL, ERROR_MALLOC);
    }
  if (key_to_cleared_args == NULL)
    {
      key_to_cleared_args = args_set_map_new(array_size(plot_merge_clear_keys));
      cleanup_and_set_error_if(hierarchy_to_id == NULL, ERROR_MALLOC);
    }
  args_values(global_root_args, "append_plots", "i", &append_plots);
  get_id_from_args(merge_args, &plot_id, &subplot_id, &series_id);
  if (plot_id > 0)
    {
      uint_map_insert(hierarchy_to_id, "plots", plot_id);
    }
  else
    {
      uint_map_insert_default(hierarchy_to_id, "plots", append_plots ? 0 : active_plot_index);
      uint_map_at(hierarchy_to_id, "plots", (unsigned int *)&plot_id);
      logger((stderr, "Using plot_id \"%u\"\n", plot_id));
    }
  if (subplot_id > 0)
    {
      uint_map_insert(hierarchy_to_id, "subplots", subplot_id);
    }
  else
    {
      uint_map_insert_default(hierarchy_to_id, "subplots", 1);
      uint_map_at(hierarchy_to_id, "subplots", (unsigned int *)&subplot_id);
    }
  if (series_id > 0)
    {
      uint_map_insert(hierarchy_to_id, "series", series_id);
    }
  else
    {
      uint_map_insert_default(hierarchy_to_id, "series", 1);
      uint_map_at(hierarchy_to_id, "series", (unsigned int *)&series_id);
    }
  /* special case: clear the plot container before usage if
   * - it is the first call of `plot_merge_args` AND `hold_always` is `0` AND
   *   - `plot_id` is `1` and `hold_plots` is not set OR
   *   - `hold_plots` is true and no plot will be appended (`plot_id` > 0)
   */
  if (strcmp(*hierarchy_name_ptr, "root") == 0 && plot_id > 0 && !hold_always)
    {
      int hold_plots_key_available, hold_plots;
      hold_plots_key_available = args_values(args, "hold_plots", "i", &hold_plots);
      if (hold_plots_key_available)
        {
          logger((stderr, "Do%s hold plots\n", hold_plots ? "" : " not"));
        }
      if ((hold_plots_key_available && !hold_plots) || (!hold_plots_key_available && plot_id == 1))
        {
          cleanup_and_set_error_if(!args_values(args, "plots", "A", &args_array), ERROR_INTERNAL);
          current_args = args_array[plot_id - 1];
          grm_args_clear(current_args);
          error = plot_init_args_structure(current_args, hierarchy_name_ptr + 1, 1);
          cleanup_if_error;
          logger((stderr, "Cleared current args\n"));
        }
      else
        {
          logger((stderr, "Held current args\n"));
        }
    }
#ifndef NDEBUG
  if (strcmp(*hierarchy_name_ptr, "root") == 0 && hold_always)
    {
      logger((stderr, "\"hold_always\" is set\n"));
    }
#endif /* ifndef  */
  merge_it = args_iter(merge_args);
  cleanup_and_set_error_if(merge_it == NULL, ERROR_MALLOC);
  while ((merge_arg = merge_it->next(merge_it)) != NULL)
    {
      if (str_equals_any_in_array(merge_arg->key, plot_merge_ignore_keys))
        {
          continue;
        }
      /* First, find the correct hierarchy level where the current merge value belongs to. */
      error = plot_get_args_in_hierarchy(args, hierarchy_name_ptr, merge_arg->key, hierarchy_to_id,
                                         (const grm_args_t **)&current_args, &current_hierarchy_name_ptr);
      if (error == ERROR_PLOT_UNKNOWN_KEY)
        {
          logger((stderr, "WARNING: The key \"%s\" is not assigned to any hierarchy level.\n", merge_arg->key));
        }
      cleanup_if_error;
      if (str_equals_any_in_array(*current_hierarchy_name_ptr, plot_merge_clear_keys))
        {
          int clear_args = 1;
          args_set_t *cleared_args = NULL;
          if (args_set_map_at(key_to_cleared_args, *current_hierarchy_name_ptr, &cleared_args))
            {
              clear_args = !args_set_contains(cleared_args, current_args);
            }
          if (clear_args)
            {
              logger((stderr, "Perform a clear on the current args container\n"));
              grm_args_clear(current_args);
              if (cleared_args == NULL)
                {
                  cleared_args = args_set_new(10); /* FIXME: do not use a magic number, use a growbable set instead! */
                  cleanup_and_set_error_if(cleared_args == NULL, ERROR_MALLOC);
                  cleanup_and_set_error_if(
                      !args_set_map_insert(key_to_cleared_args, *current_hierarchy_name_ptr, cleared_args),
                      ERROR_INTERNAL);
                }
              logger((stderr, "Add args container \"%p\" to cleared args with key \"%s\"\n", (void *)current_args,
                      *current_hierarchy_name_ptr));
              cleanup_and_set_error_if(!args_set_add(cleared_args, current_args), ERROR_INTERNAL);
            }
        }
      /* If the current key is a hierarchy key, perform a merge. Otherwise (else branch) put the value in without a
       * merge.
       */
      if (current_hierarchy_name_ptr[1] != NULL && strcmp(merge_arg->key, current_hierarchy_name_ptr[1]) == 0)
        {
          /* `args_at` cannot fail in this case because the `args` object was initialized with an empty structure
           * before. If `arg` is NULL, an internal error occurred. */
          arg = args_at(current_args, merge_arg->key);
          cleanup_and_set_error_if(arg == NULL, ERROR_INTERNAL);
          value_it = arg_value_iter(arg);
          merge_value_it = arg_value_iter(merge_arg);
          cleanup_and_set_error_if(value_it == NULL, ERROR_MALLOC);
          cleanup_and_set_error_if(merge_value_it == NULL, ERROR_MALLOC);
          /* Do not support two dimensional argument arrays like `nAnA`) -> a loop would be needed with more memory
           * management */
          cleanup_and_set_error_if(value_it->next(value_it) == NULL, ERROR_MALLOC);
          cleanup_and_set_error_if(merge_value_it->next(merge_value_it) == NULL, ERROR_MALLOC);
          /* Increase the array size of the internal args array if necessary */
          if (merge_value_it->array_length > value_it->array_length)
            {
              error = plot_init_arg_structure(arg, current_hierarchy_name_ptr, merge_value_it->array_length);
              cleanup_if_error;
              args_value_iterator_delete(value_it);
              value_it = arg_value_iter(arg);
              cleanup_and_set_error_if(value_it == NULL, ERROR_MALLOC);
              cleanup_and_set_error_if(value_it->next(value_it) == NULL, ERROR_MALLOC);
              args_array = *(grm_args_t ***)value_it->value_ptr;
            }
          else
            {
              /* The internal args container stores always an array for hierarchy levels */
              args_array = *(grm_args_t ***)value_it->value_ptr;
            }
          if (merge_value_it->is_array)
            {
              merge_args_array = *(grm_args_t ***)merge_value_it->value_ptr;
            }
          else
            {
              merge_args_array = (grm_args_t **)merge_value_it->value_ptr;
            }
          /* Merge array entries pairwise */
          for (i = 0; i < merge_value_it->array_length; ++i)
            {
              logger((stderr, "Perform a recursive merge on key \"%s\", array index \"%d\"\n", merge_arg->key, i));
              error = plot_merge_args(args_array[i], merge_args_array[i], current_hierarchy_name_ptr + 1,
                                      hierarchy_to_id, hold_always);
              cleanup_if_error;
            }
        }
      else
        {
          logger((stderr, "Perform a replace on key \"%s\"\n", merge_arg->key));
          error = args_push_arg(current_args, merge_arg);
          cleanup_if_error;
        }
    }

cleanup:
  if (recursion_level == 0)
    {
      if (key_to_cleared_args != NULL)
        {
          args_set_t *cleared_args = NULL;
          const char **current_key_ptr = plot_merge_clear_keys;
          while (*current_key_ptr != NULL)
            {
              if (args_set_map_at(key_to_cleared_args, *current_key_ptr, &cleared_args))
                {
                  args_set_delete(cleared_args);
                }
              ++current_key_ptr;
            }
          args_set_map_delete(key_to_cleared_args);
          key_to_cleared_args = NULL;
        }
    }
  if (hierarchy_to_id != NULL)
    {
      uint_map_delete(hierarchy_to_id);
      hierarchy_to_id = NULL;
    }
  if (merge_it != NULL)
    {
      args_iterator_delete(merge_it);
    }
  if (value_it != NULL)
    {
      args_value_iterator_delete(value_it);
    }
  if (merge_value_it != NULL)
    {
      args_value_iterator_delete(merge_value_it);
    }

  --recursion_level;

  return error;
}

error_t plot_init_arg_structure(arg_t *arg, const char **hierarchy_name_ptr, unsigned int next_hierarchy_level_max_id)
{
  grm_args_t **args_array = NULL;
  unsigned int args_old_array_length;
  unsigned int i;
  error_t error = NO_ERROR;

  logger((stderr, "Init plot args structure for hierarchy: \"%s\"\n", *hierarchy_name_ptr));

  ++hierarchy_name_ptr;
  if (*hierarchy_name_ptr == NULL)
    {
      return NO_ERROR;
    }
  arg_first_value(arg, "A", NULL, &args_old_array_length);
  if (next_hierarchy_level_max_id <= args_old_array_length)
    {
      return NO_ERROR;
    }
  logger((stderr, "Increase array for key \"%s\" from %d to %d\n", *hierarchy_name_ptr, args_old_array_length,
          next_hierarchy_level_max_id));
  error = arg_increase_array(arg, next_hierarchy_level_max_id - args_old_array_length);
  return_if_error;
  arg_values(arg, "A", &args_array);
  for (i = args_old_array_length; i < next_hierarchy_level_max_id; ++i)
    {
      args_array[i] = grm_args_new();
      grm_args_push(args_array[i], "array_index", "i", i);
      return_error_if(args_array[i] == NULL, ERROR_MALLOC);
      error = plot_init_args_structure(args_array[i], hierarchy_name_ptr, 1);
      return_if_error;
      if (strcmp(*hierarchy_name_ptr, "plots") == 0)
        {
          grm_args_push(args_array[i], "in_use", "i", 0);
        }
    }

  return NO_ERROR;
}

error_t plot_init_args_structure(grm_args_t *args, const char **hierarchy_name_ptr,
                                 unsigned int next_hierarchy_level_max_id)
{
  arg_t *arg = NULL;
  grm_args_t **args_array = NULL;
  unsigned int i;
  error_t error = NO_ERROR;

  logger((stderr, "Init plot args structure for hierarchy: \"%s\"\n", *hierarchy_name_ptr));

  ++hierarchy_name_ptr;
  if (*hierarchy_name_ptr == NULL)
    {
      return NO_ERROR;
    }
  arg = args_at(args, *hierarchy_name_ptr);
  if (arg == NULL)
    {
      args_array = calloc(next_hierarchy_level_max_id, sizeof(grm_args_t *));
      error_cleanup_and_set_error_if(args_array == NULL, ERROR_MALLOC);
      for (i = 0; i < next_hierarchy_level_max_id; ++i)
        {
          args_array[i] = grm_args_new();
          grm_args_push(args_array[i], "array_index", "i", i);
          error_cleanup_and_set_error_if(args_array[i] == NULL, ERROR_MALLOC);
          error = plot_init_args_structure(args_array[i], hierarchy_name_ptr, 1);
          error_cleanup_if_error;
          if (strcmp(*hierarchy_name_ptr, "plots") == 0)
            {
              grm_args_push(args_array[i], "in_use", "i", 0);
            }
        }
      error_cleanup_if(!grm_args_push(args, *hierarchy_name_ptr, "nA", next_hierarchy_level_max_id, args_array));
      free(args_array);
      args_array = NULL;
    }
  else
    {
      error = plot_init_arg_structure(arg, hierarchy_name_ptr - 1, next_hierarchy_level_max_id);
      error_cleanup_if_error;
    }

  return NO_ERROR;

error_cleanup:
  if (args_array != NULL)
    {
      for (i = 0; i < next_hierarchy_level_max_id; ++i)
        {
          if (args_array[i] != NULL)
            {
              grm_args_delete(args_array[i]);
            }
        }
      free(args_array);
    }

  return error;
}

void plot_set_flag_defaults(void)
{
  /* Use a standalone function for initializing flags instead of `plot_set_attribute_defaults` to guarantee the flags
   * are already set before `grm_plot` is called (important for `grm_merge`) */

  logger((stderr, "Set global flag defaults\n"));

  args_setdefault(global_root_args, "append_plots", "i", ROOT_DEFAULT_APPEND_PLOTS);
}

void plot_set_attribute_defaults(grm_args_t *plot_args)
{
  const char *kind;
  grm_args_t **current_subplot, **current_series;
  double garbage0, garbage1;

  logger((stderr, "Set plot attribute defaults\n"));

  args_setdefault(plot_args, "clear", "i", PLOT_DEFAULT_CLEAR);
  args_setdefault(plot_args, "update", "i", PLOT_DEFAULT_UPDATE);
  if (!grm_args_contains(plot_args, "figsize"))
    {
      args_setdefault(plot_args, "size", "dd", PLOT_DEFAULT_WIDTH, PLOT_DEFAULT_HEIGHT);
    }

  args_values(plot_args, "subplots", "A", &current_subplot);
  while (*current_subplot != NULL)
    {
      args_setdefault(*current_subplot, "kind", "s", PLOT_DEFAULT_KIND);
      args_values(*current_subplot, "kind", "s", &kind);
      if (grm_args_contains(*current_subplot, "labels"))
        {
          args_setdefault(*current_subplot, "location", "i", PLOT_DEFAULT_LOCATION);
        }
      args_setdefault(*current_subplot, "subplot", "dddd", PLOT_DEFAULT_SUBPLOT_MIN_X, PLOT_DEFAULT_SUBPLOT_MAX_X,
                      PLOT_DEFAULT_SUBPLOT_MIN_Y, PLOT_DEFAULT_SUBPLOT_MAX_Y);
      args_setdefault(*current_subplot, "xlog", "i", PLOT_DEFAULT_XLOG);
      args_setdefault(*current_subplot, "ylog", "i", PLOT_DEFAULT_YLOG);
      args_setdefault(*current_subplot, "zlog", "i", PLOT_DEFAULT_ZLOG);
      args_setdefault(*current_subplot, "xflip", "i", PLOT_DEFAULT_XFLIP);
      args_setdefault(*current_subplot, "yflip", "i", PLOT_DEFAULT_YFLIP);
      args_setdefault(*current_subplot, "zflip", "i", PLOT_DEFAULT_ZFLIP);
      if (str_equals_any(kind, 1, "heatmap"))
        {
          args_setdefault(*current_subplot, "adjust_xlim", "i", 0);
          args_setdefault(*current_subplot, "adjust_ylim", "i", 0);
        }
      else
        {
          args_setdefault(
              *current_subplot, "adjust_xlim", "i",
              (args_values(*current_subplot, "xlim", "dd", &garbage0, &garbage1) ? 0 : PLOT_DEFAULT_ADJUST_XLIM));
          args_setdefault(
              *current_subplot, "adjust_ylim", "i",
              (args_values(*current_subplot, "ylim", "dd", &garbage0, &garbage1) ? 0 : PLOT_DEFAULT_ADJUST_YLIM));
          args_setdefault(
              *current_subplot, "adjust_zlim", "i",
              (args_values(*current_subplot, "zlim", "dd", &garbage0, &garbage1) ? 0 : PLOT_DEFAULT_ADJUST_ZLIM));
        }
      args_setdefault(*current_subplot, "colormap", "i", PLOT_DEFAULT_COLORMAP);
      args_setdefault(*current_subplot, "rotation", "i", PLOT_DEFAULT_ROTATION);
      args_setdefault(*current_subplot, "tilt", "i", PLOT_DEFAULT_TILT);
      args_setdefault(*current_subplot, "keep_aspect_ratio", "i", PLOT_DEFAULT_KEEP_ASPECT_RATIO);

      if (str_equals_any(kind, 2, "contour", "contourf"))
        {
          args_setdefault(*current_subplot, "levels", "i", PLOT_DEFAULT_CONTOUR_LEVELS);
        }
      else if (strcmp(kind, "hexbin") == 0)
        {
          args_setdefault(*current_subplot, "nbins", "i", PLOT_DEFAULT_HEXBIN_NBINS);
        }
      else if (strcmp(kind, "tricont") == 0)
        {
          args_setdefault(*current_subplot, "levels", "i", PLOT_DEFAULT_TRICONT_LEVELS);
        }

      args_values(*current_subplot, "series", "A", &current_series);
      while (*current_series != NULL)
        {
          args_setdefault(*current_series, "spec", "s", SERIES_DEFAULT_SPEC);
          if (strcmp(kind, "step") == 0)
            {
              args_setdefault(*current_series, "step_where", "s", PLOT_DEFAULT_STEP_WHERE);
            }
          ++current_series;
        }
      ++current_subplot;
    }
}

void plot_pre_plot(grm_args_t *plot_args)
{
  int clear;

  logger((stderr, "Pre plot processing\n"));

  args_values(plot_args, "clear", "i", &clear);
  logger((stderr, "Got keyword \"clear\" with value %d\n", clear));
  if (clear)
    {
      gr_clearws();
    }
  plot_process_wswindow_wsviewport(plot_args);
}

void plot_process_wswindow_wsviewport(grm_args_t *plot_args)
{
  int pixel_width, pixel_height;
  int previous_pixel_width, previous_pixel_height;
  double metric_width, metric_height;
  double aspect_ratio_ws;
  double wsviewport[4] = {0.0, 0.0, 0.0, 0.0};
  double wswindow[4] = {0.0, 0.0, 0.0, 0.0};

  get_figure_size(plot_args, &pixel_width, &pixel_height, &metric_width, &metric_height);

  if (!args_values(plot_args, "previous_pixel_size", "ii", &previous_pixel_width, &previous_pixel_height) ||
      (previous_pixel_width != pixel_width || previous_pixel_height != pixel_height))
    {
      /* TODO: handle error return value? */
      event_queue_enqueue_size_event(event_queue, active_plot_index - 1, pixel_width, pixel_height);
    }

  aspect_ratio_ws = metric_width / metric_height;
  if (aspect_ratio_ws > 1)
    {
      wsviewport[1] = metric_width;
      wsviewport[3] = metric_width / aspect_ratio_ws;
      wswindow[1] = 1.0;
      wswindow[3] = 1.0 / aspect_ratio_ws;
    }
  else
    {
      wsviewport[1] = metric_height * aspect_ratio_ws;
      wsviewport[3] = metric_height;
      wswindow[1] = aspect_ratio_ws;
      wswindow[3] = 1.0;
    }

  gr_setwsviewport(wsviewport[0], wsviewport[1], wsviewport[2], wsviewport[3]);
  gr_setwswindow(wswindow[0], wswindow[1], wswindow[2], wswindow[3]);

  grm_args_push(plot_args, "wswindow", "dddd", wswindow[0], wswindow[1], wswindow[2], wswindow[3]);
  grm_args_push(plot_args, "wsviewport", "dddd", wsviewport[0], wsviewport[1], wsviewport[2], wsviewport[3]);
  grm_args_push(plot_args, "previous_pixel_size", "ii", pixel_width, pixel_height);

  logger((stderr, "Stored wswindow (%lf, %lf, %lf, %lf)\n", wswindow[0], wswindow[1], wswindow[2], wswindow[3]));
  logger(
      (stderr, "Stored wsviewport (%lf, %lf, %lf, %lf)\n", wsviewport[0], wsviewport[1], wsviewport[2], wsviewport[3]));
}

void plot_pre_subplot(grm_args_t *subplot_args)
{
  const char *kind;
  double alpha;

  logger((stderr, "Pre subplot processing\n"));

  args_values(subplot_args, "kind", "s", &kind);
  logger((stderr, "Got keyword \"kind\" with value \"%s\"\n", kind));
  if (str_equals_any(kind, 2, "imshow", "isosurface"))
    {
      plot_process_viewport(subplot_args);
    }
  else
    {
      plot_process_viewport(subplot_args);
      plot_store_coordinate_ranges(subplot_args);
      plot_process_window(subplot_args);
      if (str_equals_any(kind, 1, "polar"))
        {
          plot_draw_polar_axes(subplot_args);
        }
      else
        {
          plot_draw_axes(subplot_args, 1);
        }
    }

  plot_process_colormap(subplot_args);
  gr_uselinespec(" ");

  gr_savestate();
  if (args_values(subplot_args, "alpha", "d", &alpha))
    {
      gr_settransparency(alpha);
    }
}

void plot_process_colormap(grm_args_t *subplot_args)
{
  int colormap;

  if (args_values(subplot_args, "colormap", "i", &colormap))
    {
      gr_setcolormap(colormap);
    }
  /* TODO: Implement other datatypes for `colormap` */
}

void plot_process_viewport(grm_args_t *subplot_args)
{
  const char *kind;
  const double *subplot;
  int keep_aspect_ratio;
  double metric_width, metric_height;
  double aspect_ratio_ws;
  double vp[4];
  double viewport[4] = {0.0, 0.0, 0.0, 0.0};
  int background_color_index;

  args_values(subplot_args, "kind", "s", &kind);
  args_values(subplot_args, "subplot", "D", &subplot);
  args_values(subplot_args, "keep_aspect_ratio", "i", &keep_aspect_ratio);
  logger((stderr, "Using subplot: %lf, %lf, %lf, %lf\n", subplot[0], subplot[1], subplot[2], subplot[3]));

  get_figure_size(NULL, NULL, NULL, &metric_width, &metric_height);

  aspect_ratio_ws = metric_width / metric_height;
  memcpy(vp, subplot, sizeof(vp));
  if (aspect_ratio_ws > 1)
    {
      vp[2] /= aspect_ratio_ws;
      vp[3] /= aspect_ratio_ws;
      if (keep_aspect_ratio)
        {
          double border = 0.5 * (vp[1] - vp[0]) * (1.0 - 1.0 / aspect_ratio_ws);
          vp[0] += border;
          vp[1] -= border;
        }
    }
  else
    {
      vp[0] *= aspect_ratio_ws;
      vp[1] *= aspect_ratio_ws;
      if (keep_aspect_ratio)
        {
          double border = 0.5 * (vp[3] - vp[2]) * (1.0 - aspect_ratio_ws);
          vp[2] += border;
          vp[3] -= border;
        }
    }

  if (str_equals_any(kind, 5, "wireframe", "surface", "plot3", "scatter3", "trisurf"))
    {
      double tmp_vp[4];
      double extent;

      if (str_equals_any(kind, 2, "surface", "trisurf"))
        {
          extent = min(vp[1] - vp[0] - 0.1, vp[3] - vp[2]);
        }
      else
        {
          extent = min(vp[1] - vp[0], vp[3] - vp[2]);
        }
      tmp_vp[0] = 0.5 * (vp[0] + vp[1] - extent);
      tmp_vp[1] = 0.5 * (vp[0] + vp[1] + extent);
      tmp_vp[2] = 0.5 * (vp[2] + vp[3] - extent);
      tmp_vp[3] = 0.5 * (vp[2] + vp[3] + extent);
      memcpy(vp, tmp_vp, 4 * sizeof(double));
    }

  viewport[0] = vp[0] + 0.125 * (vp[1] - vp[0]);
  viewport[1] = vp[0] + 0.925 * (vp[1] - vp[0]);
  viewport[2] = vp[2] + 0.125 * (vp[3] - vp[2]);
  viewport[3] = vp[2] + 0.925 * (vp[3] - vp[2]);

  if (aspect_ratio_ws > 1)
    {
      viewport[2] += (1 - (subplot[3] - subplot[2]) * (subplot[3] - subplot[2])) * 0.02;
    }
  if (str_equals_any(kind, 6, "contour", "contourf", "heatmap", "nonuniformheatmap", "hexbin", "quiver"))
    {
      viewport[1] -= 0.1;
    }

  if (args_values(subplot_args, "backgroundcolor", "i", &background_color_index))
    {
      gr_savestate();
      gr_selntran(0);
      gr_setfillintstyle(GKS_K_INTSTYLE_SOLID);
      gr_setfillcolorind(background_color_index);
      if (aspect_ratio_ws > 1)
        {
          gr_fillrect(subplot[0], subplot[1], subplot[2] / aspect_ratio_ws, subplot[3] / aspect_ratio_ws);
        }
      else
        {
          gr_fillrect(subplot[0] * aspect_ratio_ws, subplot[1] * aspect_ratio_ws, subplot[2], subplot[3]);
        }
      gr_selntran(1);
      gr_restorestate();
    }

  if (strcmp(kind, "polar") == 0)
    {
      double x_center, y_center, r;

      x_center = 0.5 * (viewport[0] + viewport[1]);
      y_center = 0.5 * (viewport[2] + viewport[3]);
      r = 0.5 * min(viewport[1] - viewport[0], viewport[3] - viewport[2]);
      viewport[0] = x_center - r;
      viewport[1] = x_center + r;
      viewport[2] = y_center - r;
      viewport[3] = y_center + r;
    }

  gr_setviewport(viewport[0], viewport[1], viewport[2], viewport[3]);

  grm_args_push(subplot_args, "vp", "dddd", vp[0], vp[1], vp[2], vp[3]);
  grm_args_push(subplot_args, "viewport", "dddd", viewport[0], viewport[1], viewport[2], viewport[3]);

  logger((stderr, "Stored vp (%lf, %lf, %lf, %lf)\n", vp[0], vp[1], vp[2], vp[3]));
  logger((stderr, "Stored viewport (%lf, %lf, %lf, %lf)\n", viewport[0], viewport[1], viewport[2], viewport[3]));
}

void plot_process_window(grm_args_t *subplot_args)
{
  int scale = 0;
  const char *kind;
  int xlog, ylog, zlog;
  int xflip, yflip, zflip;
  int major_count, x_major_count, y_major_count;
  const double *stored_window;
  double x_min, x_max, y_min, y_max, z_min, z_max;
  double x, y, xzoom, yzoom;
  int adjust_xlim, adjust_ylim, adjust_zlim;
  double x_tick, y_tick;
  double x_org_low, x_org_high, y_org_low, y_org_high;
  int reset_ranges = 0;

  args_values(subplot_args, "kind", "s", &kind);
  args_values(subplot_args, "xlog", "i", &xlog);
  args_values(subplot_args, "ylog", "i", &ylog);
  args_values(subplot_args, "zlog", "i", &zlog);
  args_values(subplot_args, "xflip", "i", &xflip);
  args_values(subplot_args, "yflip", "i", &yflip);
  args_values(subplot_args, "zflip", "i", &zflip);

  if (strcmp(kind, "polar") != 0)
    {
      scale |= xlog ? GR_OPTION_X_LOG : 0;
      scale |= ylog ? GR_OPTION_Y_LOG : 0;
      scale |= zlog ? GR_OPTION_Z_LOG : 0;
      scale |= xflip ? GR_OPTION_FLIP_X : 0;
      scale |= yflip ? GR_OPTION_FLIP_Y : 0;
      scale |= zflip ? GR_OPTION_FLIP_Z : 0;
    }

  args_values(subplot_args, "xrange", "dd", &x_min, &x_max);
  args_values(subplot_args, "yrange", "dd", &y_min, &y_max);
  if (args_values(subplot_args, "reset_ranges", "i", &reset_ranges) && reset_ranges)
    {
      if (args_values(subplot_args, "original_xrange", "dd", &x_min, &x_max) &&
          args_values(subplot_args, "original_yrange", "dd", &y_min, &y_max) &&
          args_values(subplot_args, "original_adjust_xlim", "i", &adjust_xlim) &&
          args_values(subplot_args, "original_adjust_ylim", "i", &adjust_ylim))
        {
          grm_args_push(subplot_args, "xrange", "dd", x_min, x_max);
          grm_args_push(subplot_args, "yrange", "dd", y_min, y_max);
          grm_args_push(subplot_args, "adjust_xlim", "i", adjust_xlim);
          grm_args_push(subplot_args, "adjust_ylim", "i", adjust_ylim);
          grm_args_remove(subplot_args, "original_xrange");
          grm_args_remove(subplot_args, "original_yrange");
          grm_args_remove(subplot_args, "original_adjust_xlim");
          grm_args_remove(subplot_args, "original_adjust_ylim");
        }
      grm_args_remove(subplot_args, "reset_ranges");
    }
  if (grm_args_contains(subplot_args, "panzoom"))
    {
      if (!grm_args_contains(subplot_args, "original_xrange"))
        {
          grm_args_push(subplot_args, "original_xrange", "dd", x_min, x_max);
          args_values(subplot_args, "adjust_xlim", "i", &adjust_xlim);
          grm_args_push(subplot_args, "original_adjust_xlim", "i", adjust_xlim);
          grm_args_push(subplot_args, "adjust_xlim", "i", 0);
        }
      if (!grm_args_contains(subplot_args, "original_yrange"))
        {
          grm_args_push(subplot_args, "original_yrange", "dd", y_min, y_max);
          args_values(subplot_args, "adjust_ylim", "i", &adjust_ylim);
          grm_args_push(subplot_args, "original_adjust_ylim", "i", adjust_ylim);
          grm_args_push(subplot_args, "adjust_ylim", "i", 0);
        }
      if (!args_values(subplot_args, "panzoom", "dddd", &x, &y, &xzoom, &yzoom))
        {
          if (args_values(subplot_args, "panzoom", "ddd", &x, &y, &xzoom))
            {
              yzoom = xzoom;
            }
          else
            {
              /* TODO: Add error handling for type mismatch (-> next statement would fail) */
              args_values(subplot_args, "panzoom", "dd", &x, &y);
              yzoom = xzoom = 0.0;
            }
        }
      /* Ensure the correct window is set in GR */
      if (args_values(subplot_args, "window", "D", &stored_window))
        {
          gr_setwindow(stored_window[0], stored_window[1], stored_window[2], stored_window[3]);
          logger((stderr, "Window before `gr_panzoom` (%lf, %lf, %lf, %lf)\n", stored_window[0], stored_window[1],
                  stored_window[2], stored_window[3]));
        }
      gr_panzoom(x, y, xzoom, yzoom, &x_min, &x_max, &y_min, &y_max);
      logger((stderr, "Window after `gr_panzoom` (%lf, %lf, %lf, %lf)\n", x_min, x_max, y_min, y_max));
      grm_args_push(subplot_args, "xrange", "dd", x_min, x_max);
      grm_args_push(subplot_args, "yrange", "dd", y_min, y_max);
      grm_args_remove(subplot_args, "panzoom");
    }

  if (str_equals_any(kind, 6, "wireframe", "surface", "plot3", "scatter3", "polar", "trisurf"))
    {
      major_count = 2;
    }
  else
    {
      major_count = 5;
    }

  if (!(scale & GR_OPTION_X_LOG))
    {
      args_values(subplot_args, "adjust_xlim", "i", &adjust_xlim);
      if (adjust_xlim)
        {
          logger((stderr, "xrange before \"gr_adjustlimits\": (%lf, %lf)\n", x_min, x_max));
          gr_adjustlimits(&x_min, &x_max);
          logger((stderr, "xrange after \"gr_adjustlimits\": (%lf, %lf)\n", x_min, x_max));
        }
      x_major_count = major_count;
      x_tick = gr_tick(x_min, x_max) / x_major_count;
    }
  else
    {
      x_tick = x_major_count = 1;
    }
  if (!(scale & GR_OPTION_FLIP_X))
    {
      x_org_low = x_min;
      x_org_high = x_max;
    }
  else
    {
      x_org_low = x_max;
      x_org_high = x_min;
    }
  grm_args_push(subplot_args, "xtick", "d", x_tick);
  grm_args_push(subplot_args, "xorg", "dd", x_org_low, x_org_high);
  grm_args_push(subplot_args, "xmajor", "i", x_major_count);

  if (!(scale & GR_OPTION_Y_LOG))
    {
      args_values(subplot_args, "adjust_ylim", "i", &adjust_ylim);
      if (adjust_ylim)
        {
          logger((stderr, "yrange before \"gr_adjustlimits\": (%lf, %lf)\n", y_min, y_max));
          gr_adjustlimits(&y_min, &y_max);
          logger((stderr, "yrange after \"gr_adjustlimits\": (%lf, %lf)\n", y_min, y_max));
        }
      y_major_count = major_count;
      y_tick = gr_tick(y_min, y_max) / y_major_count;
    }
  else
    {
      y_tick = y_major_count = 1;
    }
  if (!(scale & GR_OPTION_FLIP_Y))
    {
      y_org_low = y_min;
      y_org_high = y_max;
    }
  else
    {
      y_org_low = y_max;
      y_org_high = y_min;
    }
  grm_args_push(subplot_args, "ytick", "d", y_tick);
  grm_args_push(subplot_args, "yorg", "dd", y_org_low, y_org_high);
  grm_args_push(subplot_args, "ymajor", "i", y_major_count);

  logger((stderr, "Storing window (%lf, %lf, %lf, %lf)\n", x_min, x_max, y_min, y_max));
  grm_args_push(subplot_args, "window", "dddd", x_min, x_max, y_min, y_max);
  if (strcmp(kind, "polar") != 0)
    {
      gr_setwindow(x_min, x_max, y_min, y_max);
    }
  else
    {
      gr_setwindow(-1.0, 1.0, -1.0, 1.0);
    }

  if (str_equals_any(kind, 5, "wireframe", "surface", "plot3", "scatter3", "trisurf"))
    {
      double z_major_count;
      double z_tick;
      double z_org_low, z_org_high;
      int rotation, tilt;

      args_values(subplot_args, "zrange", "dd", &z_min, &z_max);
      if (!(scale & GR_OPTION_Z_LOG))
        {
          args_values(subplot_args, "adjust_zlim", "i", &adjust_zlim);
          if (adjust_zlim)
            {
              logger((stderr, "zrange before \"gr_adjustlimits\": (%lf, %lf)\n", z_min, z_max));
              gr_adjustlimits(&z_min, &z_max);
              logger((stderr, "zrange after \"gr_adjustlimits\": (%lf, %lf)\n", z_min, z_max));
            }
          z_major_count = major_count;
          z_tick = gr_tick(z_min, z_max) / z_major_count;
        }
      else
        {
          z_tick = z_major_count = 1;
        }
      if (!(scale & GR_OPTION_FLIP_Z))
        {
          z_org_low = z_min;
          z_org_high = z_max;
        }
      else
        {
          z_org_low = z_max;
          z_org_high = z_min;
        }
      grm_args_push(subplot_args, "ztick", "d", z_tick);
      grm_args_push(subplot_args, "zorg", "dd", z_org_low, z_org_high);
      grm_args_push(subplot_args, "zmajor", "i", z_major_count);

      args_values(subplot_args, "rotation", "i", &rotation);
      args_values(subplot_args, "tilt", "i", &tilt);
      gr_setspace(z_min, z_max, rotation, tilt);
    }

  grm_args_push(subplot_args, "scale", "i", scale);
  gr_setscale(scale);
}

void plot_store_coordinate_ranges(grm_args_t *subplot_args)
{
  const char *kind;
  const char *fmt;
  grm_args_t **current_series;
  unsigned int series_count;
  const char *data_component_names[] = {"x", "y", "z", "c", NULL};
  const char **current_component_name;
  double *current_component = NULL;
  unsigned int point_count = 0;
  const char *range_keys[][2] = {{"xlim", "xrange"}, {"ylim", "yrange"}, {"zlim", "zrange"}, {"clim", "crange"}};
  const char *(*current_range_keys)[2];
  unsigned int i;

  logger((stderr, "Storing coordinate ranges\n"));
  /* TODO: support that single `lim` values are `null` / unset! */

  args_values(subplot_args, "kind", "s", &kind);
  string_map_at(fmt_map, kind, (char **)&fmt); /* TODO: check if the map access was successful */
  current_range_keys = range_keys;
  current_component_name = data_component_names;
  while (*current_component_name != NULL)
    {
      double min_component = DBL_MAX;
      double max_component = -DBL_MAX;
      double step = -DBL_MAX;
      if (strchr(fmt, **current_component_name) == NULL || grm_args_contains(subplot_args, (*current_range_keys)[1]))
        {
          ++current_range_keys;
          ++current_component_name;
          continue;
        }
      if (!grm_args_contains(subplot_args, (*current_range_keys)[0]))
        {
          args_first_value(subplot_args, "series", "A", &current_series, &series_count);
          while (*current_series != NULL)
            {
              if (args_first_value(*current_series, *current_component_name, "D", &current_component, &point_count))
                {
                  for (i = 0; i < point_count; i++)
                    {
                      min_component = min(current_component[i], min_component);
                      max_component = max(current_component[i], max_component);
                    }
                }
              ++current_series;
            }
          if (strcmp(kind, "quiver") == 0)
            {
              step = max(find_max_step(point_count, current_component), step);
              if (step > 0.0)
                {
                  min_component -= step;
                  max_component += step;
                }
              else if (strcmp(kind, "heatmap") == 0 && str_equals_any(*current_component_name, 2, "x", "y"))
                {
                  min_component -= 0.5;
                  max_component += 0.5;
                }
              else if ((strcmp(kind, "hist") == 0 || strcmp(kind, "barplot") == 0) &&
                       strcmp("y", *current_component_name) == 0)
                {
                  min_component = 0;
                }
            }
        }
      else
        {
          args_values(subplot_args, (*current_range_keys)[0], "dd", &min_component, &max_component);
        }
      /* TODO: This may be obsolete when all supported format-strings are added
       color is an optional part of the format strings */
      if (!(min_component == DBL_MAX && max_component == -DBL_MAX && strcmp(*current_component_name, "c") == 0))
        {
          grm_args_push(subplot_args, (*current_range_keys)[1], "dd", min_component, max_component);
        }
      ++current_range_keys;
      ++current_component_name;
    }
  /* For quiver plots use u^2 + v^2 as z value */
  if (strcmp(kind, "quiver") == 0)
    {
      double min_component = DBL_MAX;
      double max_component = -DBL_MAX;
      if (!grm_args_contains(subplot_args, "zlim"))
        {
          double *u, *v;
          /* TODO: Support more than one series? */
          /* TODO: `ERROR_PLOT_COMPONENT_LENGTH_MISMATCH` */
          args_values(subplot_args, "series", "A", &current_series);
          args_first_value(*current_series, "u", "D", &u, &point_count);
          args_first_value(*current_series, "v", "D", &v, NULL);
          for (i = 0; i < point_count; i++)
            {
              double z = u[i] * u[i] + v[i] * v[i];
              min_component = min(z, min_component);
              max_component = max(z, max_component);
            }
          min_component = sqrt(min_component);
          max_component = sqrt(max_component);
        }
      else
        {
          args_values(subplot_args, "zlim", "dd", &min_component, &max_component);
        }
      grm_args_push(subplot_args, "zrange", "dd", min_component, max_component);
    }
}

void plot_post_plot(grm_args_t *plot_args)
{
  int update;

  logger((stderr, "Post plot processing\n"));

  args_values(plot_args, "update", "i", &update);
  logger((stderr, "Got keyword \"update\" with value %d\n", update));
  if (update)
    {
      gr_updatews();
    }
}

void plot_post_subplot(grm_args_t *subplot_args)
{
  const char *kind;

  logger((stderr, "Post subplot processing\n"));

  gr_restorestate();
  args_values(subplot_args, "kind", "s", &kind);
  logger((stderr, "Got keyword \"kind\" with value \"%s\"\n", kind));
  if (str_equals_any(kind, 4, "line", "step", "scatter", "stem") && grm_args_contains(subplot_args, "labels"))
    {
      plot_draw_legend(subplot_args);
    }
}

error_t plot_get_args_in_hierarchy(grm_args_t *args, const char **hierarchy_name_start_ptr, const char *key,
                                   uint_map_t *hierarchy_to_id, const grm_args_t **found_args,
                                   const char ***found_hierarchy_name_ptr)
{
  const char *key_hierarchy_name, **current_hierarchy_name_ptr;
  grm_args_t *current_args, **args_array;
  arg_t *current_arg;
  unsigned int args_array_length, current_id;

  logger((stderr, "Check hierarchy level for key \"%s\"...\n", key));
  return_error_if(!string_map_at(plot_valid_keys_map, key, (char **)&key_hierarchy_name), ERROR_PLOT_UNKNOWN_KEY);
  logger((stderr, "... got hierarchy \"%s\"\n", key_hierarchy_name));
  current_hierarchy_name_ptr = hierarchy_name_start_ptr;
  current_args = args;
  if (strcmp(*hierarchy_name_start_ptr, key_hierarchy_name) != 0)
    {
      while (*++current_hierarchy_name_ptr != NULL)
        {
          current_arg = args_at(current_args, *current_hierarchy_name_ptr);
          return_error_if(current_arg == NULL, ERROR_INTERNAL);
          arg_first_value(current_arg, "A", &args_array, &args_array_length);
          uint_map_at(hierarchy_to_id, *current_hierarchy_name_ptr, &current_id);
          /* Check for the invalid id 0 because id 0 is set for append mode */
          if (current_id == 0)
            {
              current_id = args_array_length + 1;
              if (strcmp(*current_hierarchy_name_ptr, "plots") == 0)
                {
                  int last_plot_in_use = 0;
                  if (args_values(args_array[args_array_length - 1], "in_use", "i", &last_plot_in_use) &&
                      !last_plot_in_use)
                    {
                      /* Use the last already existing plot if it is still empty */
                      --current_id;
                    }
                }
              logger((stderr, "Append mode, set id to \"%u\"\n", current_id));
              uint_map_insert(hierarchy_to_id, *current_hierarchy_name_ptr, current_id);
            }
          if (current_id > args_array_length)
            {
              plot_init_args_structure(current_args, current_hierarchy_name_ptr - 1, current_id);
              arg_first_value(current_arg, "A", &args_array, &args_array_length);
            }
          current_args = args_array[current_id - 1];
          if (strcmp(*current_hierarchy_name_ptr, "plots") == 0)
            {
              int in_use;
              error_t error = NO_ERROR;
              args_values(current_args, "in_use", "i", &in_use);
              if (in_use)
                {
                  error = event_queue_enqueue_update_plot_event(event_queue, current_id - 1);
                }
              else
                {
                  error = event_queue_enqueue_new_plot_event(event_queue, current_id - 1);
                }
              return_if_error;
              grm_args_push(current_args, "in_use", "i", 1);
            }
          if (strcmp(*current_hierarchy_name_ptr, key_hierarchy_name) == 0)
            {
              break;
            }
        }
      return_error_if(*current_hierarchy_name_ptr == NULL, ERROR_INTERNAL);
    }
  if (found_args != NULL)
    {
      *found_args = current_args;
    }
  if (found_hierarchy_name_ptr != NULL)
    {
      *found_hierarchy_name_ptr = current_hierarchy_name_ptr;
    }

  return NO_ERROR;
}


/* ~~~~~~~~~~~~~~~~~~~~~~~~~ plotting ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

error_t plot_line(grm_args_t *subplot_args)
{
  grm_args_t **current_series;

  args_values(subplot_args, "series", "A", &current_series);
  while (*current_series != NULL)
    {
      double *x, *y;
      unsigned int x_length, y_length;
      char *spec;
      int mask;
      return_error_if(!args_first_value(*current_series, "x", "D", &x, &x_length), ERROR_PLOT_MISSING_DATA);
      return_error_if(!args_first_value(*current_series, "y", "D", &y, &y_length), ERROR_PLOT_MISSING_DATA);
      return_error_if(x_length != y_length, ERROR_PLOT_COMPONENT_LENGTH_MISMATCH);
      args_values(*current_series, "spec", "s", &spec); /* `spec` is always set */
      mask = gr_uselinespec(spec);
      if (int_equals_any(mask, 5, 0, 1, 3, 4, 5))
        {
          gr_polyline(x_length, x, y);
        }
      if (mask & 2)
        {
          gr_polymarker(x_length, x, y);
        }
      ++current_series;
    }

  return NO_ERROR;
}

error_t plot_step(grm_args_t *subplot_args)
{
  /*
   * Parameters:
   * `x` as double array
   * `y` as double array
   * optional step position `step_where` as string, modes: `pre`, `mid`, `post`, Default: `mid`
   * optional `spec`
   */
  grm_args_t **current_series;

  args_values(subplot_args, "series", "A", &current_series);
  while (*current_series != NULL)
    {
      double *x, *y, *x_step_boundaries = NULL, *y_step_values = NULL;
      unsigned int x_length, y_length, mask, i;
      char *spec;
      return_error_if(!args_first_value(*current_series, "x", "D", &x, &x_length) && x_length < 1,
                      ERROR_PLOT_MISSING_DATA);
      return_error_if(!args_first_value(*current_series, "y", "D", &y, &y_length), ERROR_PLOT_MISSING_DATA);
      return_error_if(x_length != y_length, ERROR_PLOT_COMPONENT_LENGTH_MISMATCH);
      args_values(*current_series, "spec", "s", &spec); /* `spec` is always set */
      mask = gr_uselinespec(spec);
      if (int_equals_any(mask, 5, 0, 1, 3, 4, 5))
        {
          const char *where;
          args_values(*current_series, "step_where", "s", &where); /* `spec` is always set */
          if (strcmp(where, "pre") == 0)
            {
              x_step_boundaries = calloc(2 * x_length - 1, sizeof(double));
              y_step_values = calloc(2 * x_length - 1, sizeof(double));
              x_step_boundaries[0] = x[0];
              for (i = 1; i < 2 * x_length - 2; i += 2)
                {
                  x_step_boundaries[i] = x[i / 2];
                  x_step_boundaries[i + 1] = x[i / 2 + 1];
                }
              y_step_values[0] = y[0];
              for (i = 1; i < 2 * x_length - 1; i += 2)
                {
                  y_step_values[i] = y_step_values[i + 1] = y[i / 2 + 1];
                }
              gr_polyline(2 * x_length - 1, x_step_boundaries, y_step_values);
            }
          else if (strcmp(where, "post") == 0)
            {
              x_step_boundaries = calloc(2 * x_length - 1, sizeof(double));
              y_step_values = calloc(2 * x_length - 1, sizeof(double));
              for (i = 0; i < 2 * x_length - 2; i += 2)
                {
                  x_step_boundaries[i] = x[i / 2];
                  x_step_boundaries[i + 1] = x[i / 2 + 1];
                }
              x_step_boundaries[2 * x_length - 2] = x[x_length - 1];
              for (i = 0; i < 2 * x_length - 2; i += 2)
                {
                  y_step_values[i] = y_step_values[i + 1] = y[i / 2];
                }
              y_step_values[2 * x_length - 2] = y[x_length - 1];
              gr_polyline(2 * x_length - 1, x_step_boundaries, y_step_values);
            }
          else if (strcmp(where, "mid") == 0)
            {
              x_step_boundaries = calloc(2 * x_length, sizeof(double));
              y_step_values = calloc(2 * x_length, sizeof(double));
              x_step_boundaries[0] = x[0];
              for (i = 1; i < 2 * x_length - 2; i += 2)
                {
                  x_step_boundaries[i] = x_step_boundaries[i + 1] = (x[i / 2] + x[i / 2 + 1]) / 2.0;
                }
              x_step_boundaries[2 * x_length - 1] = x[x_length - 1];
              for (i = 0; i < 2 * x_length - 1; i += 2)
                {
                  y_step_values[i] = y_step_values[i + 1] = y[i / 2];
                }
              gr_polyline(2 * x_length, x_step_boundaries, y_step_values);
            }
          free(x_step_boundaries);
          free(y_step_values);
        }
      if (mask & 2)
        {
          gr_polymarker(x_length, x, y);
        }
      ++current_series;
    }

  return NO_ERROR;
}

error_t plot_scatter(grm_args_t *subplot_args)
{
  /*
   * Parameters:
   * `x` as double array
   * `y` as double array
   * optional marker size `z` as double array
   * optional marker color `c` as double array for each marker or as single integer for all markers
   * optional `markertype` as integer (see: [Marker types](https://gr-framework.org/markertypes.html?highlight=marker))
   */
  grm_args_t **current_series;
  int *previous_marker_type = plot_scatter_markertypes;
  args_values(subplot_args, "series", "A", &current_series);
  while (*current_series != NULL)
    {
      double *x = NULL, *y = NULL, *z = NULL, *c = NULL, c_min, c_max;
      unsigned int x_length, y_length, z_length, c_length;
      int i, c_index = -1, markertype;
      return_error_if(!args_first_value(*current_series, "x", "D", &x, &x_length), ERROR_PLOT_MISSING_DATA);
      return_error_if(!args_first_value(*current_series, "y", "D", &y, &y_length), ERROR_PLOT_MISSING_DATA);
      return_error_if(x_length != y_length, ERROR_PLOT_COMPONENT_LENGTH_MISMATCH);
      if (args_first_value(*current_series, "z", "D", &z, &z_length))
        {
          return_error_if(x_length != z_length, ERROR_PLOT_COMPONENT_LENGTH_MISMATCH);
        }
      if (args_values(*current_series, "markertype", "i", &markertype))
        {
          gr_setmarkertype(markertype);
        }
      else
        {
          gr_setmarkertype(*previous_marker_type++);
          if (*previous_marker_type == INT_MAX)
            {
              previous_marker_type = plot_scatter_markertypes;
            }
        }
      if (!args_first_value(*current_series, "c", "D", &c, &c_length) &&
          args_values(*current_series, "c", "i", &c_index))
        {
          if (c_index < 0)
            {
              logger((stderr, "Invalid scatter color %d, using 0 instead\n", c_index));
              c_index = 0;
            }
          else if (c_index > 255)
            {
              logger((stderr, "Invalid scatter color %d, using 255 instead\n", c_index));
              c_index = 255;
            }
        }
      if (z != NULL || c != NULL)
        {
          args_values(subplot_args, "crange", "dd", &c_min, &c_max);
          for (i = 0; i < x_length; i++)
            {
              if (z != NULL)
                {
                  if (i < z_length)
                    {
                      gr_setmarkersize(z[i] / 100.0);
                    }
                  else
                    {
                      gr_setmarkersize(2.0);
                    }
                }
              if (c != NULL)
                {
                  if (i < c_length)
                    {
                      c_index = 1000 + (int)(255 * (c[i] - c_min) / (c_max - c_min));
                      if (c_index < 1000 || c_index > 1255)
                        {
                          continue;
                        }
                    }
                  else
                    {
                      c_index = 989;
                    }
                  gr_setmarkercolorind(c_index);
                }
              else if (c_index != -1)
                {
                  gr_setmarkercolorind(1000 + c_index);
                }
              gr_polymarker(1, &x[i], &y[i]);
            }
        }
      else
        {
          gr_polymarker(x_length, x, y);
        }
      ++current_series;
    }

  return NO_ERROR;
}

error_t plot_quiver(grm_args_t *subplot_args)
{
  grm_args_t **current_series;

  args_values(subplot_args, "series", "A", &current_series);
  while (*current_series != NULL)
    {
      double *x = NULL, *y = NULL, *u = NULL, *v = NULL;
      unsigned int x_length, y_length, u_length, v_length;
      return_error_if(!args_first_value(*current_series, "x", "D", &x, &x_length), ERROR_PLOT_MISSING_DATA);
      return_error_if(!args_first_value(*current_series, "y", "D", &y, &y_length), ERROR_PLOT_MISSING_DATA);
      return_error_if(!args_first_value(*current_series, "u", "D", &u, &u_length), ERROR_PLOT_MISSING_DATA);
      return_error_if(!args_first_value(*current_series, "v", "D", &v, &v_length), ERROR_PLOT_MISSING_DATA);
      return_error_if(x_length != y_length, ERROR_PLOT_COMPONENT_LENGTH_MISMATCH);
      /* TODO: Check length of `u` and `v` */
      gr_quiver(x_length, y_length, x, y, u, v, 1);

      ++current_series;
    }

  return NO_ERROR;
}

error_t plot_stem(grm_args_t *subplot_args)
{
  const double *window;
  double base_line_y[2] = {0.0, 0.0};
  double stem_x[2], stem_y[2] = {0.0};
  grm_args_t **current_series;

  args_values(subplot_args, "window", "D", &window);
  args_values(subplot_args, "series", "A", &current_series);
  while (*current_series != NULL)
    {
      double *x, *y;
      unsigned int x_length, y_length;
      char *spec;
      unsigned int i;
      return_error_if(!args_first_value(*current_series, "x", "D", &x, &x_length), ERROR_PLOT_MISSING_DATA);
      return_error_if(!args_first_value(*current_series, "y", "D", &y, &y_length), ERROR_PLOT_MISSING_DATA);
      return_error_if(x_length != y_length, ERROR_PLOT_COMPONENT_LENGTH_MISMATCH);
      gr_polyline(2, (double *)window, base_line_y);
      gr_setmarkertype(GKS_K_MARKERTYPE_SOLID_CIRCLE);
      args_values(*current_series, "spec", "s", &spec);
      gr_uselinespec(spec);
      for (i = 0; i < x_length; ++i)
        {
          stem_x[0] = stem_x[1] = x[i];
          stem_y[1] = y[i];
          gr_polyline(2, stem_x, stem_y);
        }
      gr_polymarker(x_length, x, y);
      ++current_series;
    }

  return NO_ERROR;
}

error_t plot_hist(grm_args_t *subplot_args)
{
  const double *window;
  double y_min;
  grm_args_t **current_series;

  args_values(subplot_args, "window", "D", &window);
  y_min = window[2];
  args_values(subplot_args, "series", "A", &current_series);
  while (*current_series != NULL)
    {
      double *x, *y;
      unsigned int x_length, y_length;
      unsigned int i;
      return_error_if(!args_first_value(*current_series, "x", "D", &x, &x_length), ERROR_PLOT_MISSING_DATA);
      return_error_if(!args_first_value(*current_series, "y", "D", &y, &y_length), ERROR_PLOT_MISSING_DATA);
      return_error_if(x_length != y_length, ERROR_PLOT_COMPONENT_LENGTH_MISMATCH);
      for (i = 0; i <= y_length; ++i)
        {
          gr_setfillcolorind(989);
          gr_setfillintstyle(GKS_K_INTSTYLE_SOLID);
          gr_fillrect(x[i - 1], x[i], y_min, y[i - 1]);
          gr_setfillcolorind(1);
          gr_setfillintstyle(GKS_K_INTSTYLE_HOLLOW);
          gr_fillrect(x[i - 1], x[i], y_min, y[i - 1]);
        }
      ++current_series;
    }

  return NO_ERROR;
}

error_t plot_barplot(grm_args_t *subplot_args)
{
  const double *window;
  grm_args_t **current_series;

  args_values(subplot_args, "window", "D", &window);
  args_values(subplot_args, "series", "A", &current_series);
  while (*current_series != NULL)
    {
      double *x, *y;
      unsigned int x_length, y_length;
      unsigned int i;
      return_error_if(!args_first_value(*current_series, "x", "D", &x, &x_length), ERROR_PLOT_MISSING_DATA);
      return_error_if(!args_first_value(*current_series, "y", "D", &y, &y_length), ERROR_PLOT_MISSING_DATA);
      return_error_if(x_length != y_length + 1, ERROR_PLOT_COMPONENT_LENGTH_MISMATCH);
      for (i = 1; i <= y_length; ++i)
        {
          gr_setfillcolorind(989);
          gr_setfillintstyle(GKS_K_INTSTYLE_SOLID);
          gr_fillrect(x[i - 1], x[i], 0, y[i - 1]);
          gr_setfillcolorind(1);
          gr_setfillintstyle(GKS_K_INTSTYLE_HOLLOW);
          gr_fillrect(x[i - 1], x[i], 0, y[i - 1]);
        }
      ++current_series;
    }

  return NO_ERROR;
}

error_t plot_contour(grm_args_t *subplot_args)
{
  double z_min, z_max;
  int num_levels;
  double *h;
  double *gridit_x = NULL, *gridit_y = NULL, *gridit_z = NULL;
  grm_args_t **current_series;
  int i;
  error_t error = NO_ERROR;

  args_values(subplot_args, "zrange", "dd", &z_min, &z_max);
  gr_setspace(z_min, z_max, 0, 90);
  args_values(subplot_args, "levels", "i", &num_levels);
  h = malloc(num_levels * sizeof(double));
  if (h == NULL)
    {
      debug_print_malloc_error();
      error = ERROR_MALLOC;
      goto cleanup;
    }
  args_values(subplot_args, "series", "A", &current_series);
  while (*current_series != NULL)
    {
      double *x, *y, *z;
      unsigned int x_length, y_length, z_length;
      args_first_value(*current_series, "x", "D", &x, &x_length);
      args_first_value(*current_series, "y", "D", &y, &y_length);
      args_first_value(*current_series, "z", "D", &z, &z_length);
      if (x_length == y_length && x_length == z_length)
        {
          if (gridit_x == NULL)
            {
              gridit_x = malloc(PLOT_CONTOUR_GRIDIT_N * sizeof(double));
              gridit_y = malloc(PLOT_CONTOUR_GRIDIT_N * sizeof(double));
              gridit_z = malloc(PLOT_CONTOUR_GRIDIT_N * PLOT_CONTOUR_GRIDIT_N * sizeof(double));
              if (gridit_x == NULL || gridit_y == NULL || gridit_z == NULL)
                {
                  debug_print_malloc_error();
                  error = ERROR_MALLOC;
                  goto cleanup;
                }
            }
          gr_gridit(x_length, x, y, z, PLOT_CONTOUR_GRIDIT_N, PLOT_CONTOUR_GRIDIT_N, gridit_x, gridit_y, gridit_z);
          for (i = 0; i < PLOT_CONTOUR_GRIDIT_N * PLOT_CONTOUR_GRIDIT_N; i++)
            {
              z_min = min(gridit_z[i], z_min);
              z_max = max(gridit_z[i], z_max);
            }
          for (i = 0; i < num_levels; ++i)
            {
              h[i] = z_min + (1.0 * i) / num_levels * (z_max - z_min);
            }
          gr_contour(PLOT_CONTOUR_GRIDIT_N, PLOT_CONTOUR_GRIDIT_N, num_levels, gridit_x, gridit_y, h, gridit_z, 1000);
        }
      else
        {
          if (x_length * y_length != z_length)
            {
              error = ERROR_PLOT_COMPONENT_LENGTH_MISMATCH;
              goto cleanup;
            }
          for (i = 0; i < num_levels; ++i)
            {
              h[i] = z_min + (1.0 * i) / num_levels * (z_max - z_min);
            }
          gr_contour(x_length, y_length, num_levels, x, y, h, z, 1000);
        }
      ++current_series;
    }
  if ((error = plot_draw_colorbar(subplot_args, 0.0, num_levels)) != NO_ERROR)
    {
      goto cleanup;
    }

cleanup:
  free(h);
  free(gridit_x);
  free(gridit_y);
  free(gridit_z);

  return error;
}

error_t plot_contourf(grm_args_t *subplot_args)
{
  double z_min, z_max;
  int num_levels, scale;
  double *h;
  double *gridit_x = NULL, *gridit_y = NULL, *gridit_z = NULL;
  grm_args_t **current_series;
  int i;
  error_t error = NO_ERROR;

  args_values(subplot_args, "zrange", "dd", &z_min, &z_max);
  gr_setspace(z_min, z_max, 0, 90);
  args_values(subplot_args, "levels", "i", &num_levels);
  h = malloc(num_levels * sizeof(double));
  if (h == NULL)
    {
      debug_print_malloc_error();
      error = ERROR_MALLOC;
      goto cleanup;
    }
  args_values(subplot_args, "scale", "i", &scale);
  gr_setscale(scale);
  args_values(subplot_args, "series", "A", &current_series);
  while (*current_series != NULL)
    {
      double *x, *y, *z;
      unsigned int x_length, y_length, z_length;
      args_first_value(*current_series, "x", "D", &x, &x_length);
      args_first_value(*current_series, "y", "D", &y, &y_length);
      args_first_value(*current_series, "z", "D", &z, &z_length);
      if ((error = plot_draw_colorbar(subplot_args, 0.0, num_levels)) != NO_ERROR)
        {
          goto cleanup;
        }
      gr_setlinecolorind(1);
      if (x_length == y_length && x_length == z_length)
        {
          if (gridit_x == NULL)
            {
              gridit_x = malloc(PLOT_CONTOUR_GRIDIT_N * sizeof(double));
              gridit_y = malloc(PLOT_CONTOUR_GRIDIT_N * sizeof(double));
              gridit_z = malloc(PLOT_CONTOUR_GRIDIT_N * PLOT_CONTOUR_GRIDIT_N * sizeof(double));
              if (gridit_x == NULL || gridit_y == NULL || gridit_z == NULL)
                {
                  debug_print_malloc_error();
                  error = ERROR_MALLOC;
                  goto cleanup;
                }
            }
          gr_gridit(x_length, x, y, z, PLOT_CONTOUR_GRIDIT_N, PLOT_CONTOUR_GRIDIT_N, gridit_x, gridit_y, gridit_z);
          for (i = 0; i < PLOT_CONTOUR_GRIDIT_N * PLOT_CONTOUR_GRIDIT_N; i++)
            {
              z_min = min(gridit_z[i], z_min);
              z_max = max(gridit_z[i], z_max);
            }
          for (i = 0; i < num_levels; ++i)
            {
              h[i] = z_min + (1.0 * i) / num_levels * (z_max - z_min);
            }
          gr_contourf(PLOT_CONTOUR_GRIDIT_N, PLOT_CONTOUR_GRIDIT_N, num_levels, gridit_x, gridit_y, h, gridit_z, 1000);
        }
      else
        {
          if (x_length * y_length != z_length)
            {
              error = ERROR_PLOT_COMPONENT_LENGTH_MISMATCH;
              goto cleanup;
            }
          for (i = 0; i < num_levels; ++i)
            {
              h[i] = z_min + (1.0 * i) / num_levels * (z_max - z_min);
            }
          gr_contourf(x_length, y_length, num_levels, x, y, h, z, 1000);
        }
      ++current_series;
    }

cleanup:
  free(h);
  free(gridit_x);
  free(gridit_y);
  free(gridit_z);

  return error;
}

error_t plot_hexbin(grm_args_t *subplot_args)
{
  int nbins;
  grm_args_t **current_series;

  args_values(subplot_args, "nbins", "i", &nbins);
  args_values(subplot_args, "series", "A", &current_series);
  while (*current_series != NULL)
    {
      double *x, *y;
      unsigned int x_length, y_length;
      int cntmax;
      return_error_if(!args_first_value(*current_series, "x", "D", &x, &x_length), ERROR_PLOT_MISSING_DATA);
      return_error_if(!args_first_value(*current_series, "y", "D", &y, &y_length), ERROR_PLOT_MISSING_DATA);
      return_error_if(x_length != y_length, ERROR_PLOT_COMPONENT_LENGTH_MISMATCH);
      cntmax = gr_hexbin(x_length, x, y, nbins);
      /* TODO: return an error in the else case? */
      if (cntmax > 0)
        {
          grm_args_push(subplot_args, "zrange", "dd", 0.0, 1.0 * cntmax);
          plot_draw_colorbar(subplot_args, 0.0, 256);
        }
      ++current_series;
    }

  return NO_ERROR;
}

error_t plot_heatmap(grm_args_t *subplot_args)
{
  const char *kind = NULL;
  grm_args_t **current_series;
  int i, zlim_set, icmap[256], *rgba, *data, zlog;
  unsigned int width, height, z_length;
  double *x, *y, *z, z_min, z_max, c_min, c_max, tmp, zv;

  args_values(subplot_args, "series", "A", &current_series);
  args_values(subplot_args, "kind", "s", &kind);
  return_error_if(!args_first_value(*current_series, "z", "D", &z, &z_length), ERROR_PLOT_MISSING_DATA);
  return_error_if(!args_first_value(*current_series, "x", "D", &x, &width), ERROR_PLOT_MISSING_DATA);
  return_error_if(!args_first_value(*current_series, "y", "D", &y, &height), ERROR_PLOT_MISSING_DATA);
  args_values(subplot_args, "zrange", "dd", &z_min, &z_max);
  if (!args_values(subplot_args, "zlog", "i", &zlog))
    {
      zlog = 0;
    }

  if (zlog)
    {
      z_min = log(z_min);
      z_max = log(z_max);
    }

  if (!args_values(subplot_args, "crange", "dd", &c_min, &c_max))
    {
      c_min = z_min;
      c_max = z_max;
    }
  if (zlog)
    {
      c_min = log(c_min);
      c_max = log(c_max);
    }

  if (str_equals_any(kind, 1, "nonuniformheatmap"))
    {
      --width;
      --height;
    }
  for (i = 0; i < 256; i++)
    {
      gr_inqcolor(1000 + i, icmap + i);
    }

  data = malloc(height * width * sizeof(int));
  if (z_max > z_min)
    {
      for (i = 0; i < width * height; i++)
        {
          if (zlog)
            {
              zv = log(z[i]);
            }
          else
            {
              zv = z[i];
            }

          if (zv > z_max || zv < z_min)
            {
              data[i] = -1;
            }
          else
            {
              data[i] = (int)((zv - c_min) / (c_max - c_min) * 255);
              if (data[i] >= 255)
                {
                  data[i] = 255;
                }
              else if (data[i] < 0)
                {
                  data[i] = 0;
                }
            }
        }
    }
  else
    {
      for (i = 0; i < width * height; i++)
        {
          data[i] = 0;
        }
    }
  rgba = malloc(height * width * sizeof(int));
  if (str_equals_any(kind, 1, "heatmap"))
    {
      for (i = 0; i < height * width; i++)
        {
          if (data[i] == -1)
            {
              rgba[i] = 0;
            }
          else
            {
              rgba[i] = (255 << 24) + icmap[data[i]];
            }
        }
      gr_drawimage(0.5, width + 0.5, height + 0.5, 0.5, width, height, rgba, 0);
    }
  else
    {
      for (i = 0; i < height * width; i++)
        {
          if (data[i] == -1)
            {
              rgba[i] = 1256 + 1; /* Invalid color index -> gr_nonuniformcellarray draws a transparent rectangle */
            }
          else
            {
              rgba[i] = data[i] + 1000;
            }
        }
      gr_nonuniformcellarray(x, y, width, height, 1, 1, width, height, rgba);
    }
  free(rgba);
  free(data);

  plot_draw_colorbar(subplot_args, 0.0, 256);
  return NO_ERROR;
}

error_t plot_wireframe(grm_args_t *subplot_args)
{
  double *gridit_x = NULL, *gridit_y = NULL, *gridit_z = NULL;
  grm_args_t **current_series;
  error_t error = NO_ERROR;

  args_values(subplot_args, "series", "A", &current_series);
  while (*current_series != NULL)
    {
      double *x, *y, *z;
      unsigned int x_length, y_length, z_length;
      args_first_value(*current_series, "x", "D", &x, &x_length);
      args_first_value(*current_series, "y", "D", &y, &y_length);
      args_first_value(*current_series, "z", "D", &z, &z_length);
      gr_setfillcolorind(0);
      if (x_length == y_length && x_length == z_length)
        {
          if (gridit_x == NULL)
            {
              gridit_x = malloc(PLOT_WIREFRAME_GRIDIT_N * sizeof(double));
              gridit_y = malloc(PLOT_WIREFRAME_GRIDIT_N * sizeof(double));
              gridit_z = malloc(PLOT_WIREFRAME_GRIDIT_N * PLOT_WIREFRAME_GRIDIT_N * sizeof(double));
              if (gridit_x == NULL || gridit_y == NULL || gridit_z == NULL)
                {
                  debug_print_malloc_error();
                  error = ERROR_MALLOC;
                  goto cleanup;
                }
            }
          gr_gridit(x_length, x, y, z, PLOT_WIREFRAME_GRIDIT_N, PLOT_WIREFRAME_GRIDIT_N, gridit_x, gridit_y, gridit_z);
          gr_surface(PLOT_WIREFRAME_GRIDIT_N, PLOT_WIREFRAME_GRIDIT_N, gridit_x, gridit_y, gridit_z,
                     GR_OPTION_FILLED_MESH);
        }
      else
        {
          if (x_length * y_length != z_length)
            {
              error = ERROR_PLOT_COMPONENT_LENGTH_MISMATCH;
              goto cleanup;
            }
          gr_surface(x_length, y_length, x, y, z, GR_OPTION_FILLED_MESH);
        }
      ++current_series;
    }
  plot_draw_axes(subplot_args, 2);

cleanup:
  free(gridit_x);
  free(gridit_y);
  free(gridit_z);

  return error;
}

error_t plot_surface(grm_args_t *subplot_args)
{
  double *gridit_x = NULL, *gridit_y = NULL, *gridit_z = NULL;
  grm_args_t **current_series;
  error_t error = NO_ERROR;

  args_values(subplot_args, "series", "A", &current_series);
  while (*current_series != NULL)
    {
      double *x, *y, *z;
      unsigned int x_length, y_length, z_length;
      args_first_value(*current_series, "x", "D", &x, &x_length);
      args_first_value(*current_series, "y", "D", &y, &y_length);
      args_first_value(*current_series, "z", "D", &z, &z_length);
      /* TODO: add support for GR3 */
      if (x_length == y_length && x_length == z_length)
        {
          if (gridit_x == NULL)
            {
              gridit_x = malloc(PLOT_SURFACE_GRIDIT_N * sizeof(double));
              gridit_y = malloc(PLOT_SURFACE_GRIDIT_N * sizeof(double));
              gridit_z = malloc(PLOT_SURFACE_GRIDIT_N * PLOT_SURFACE_GRIDIT_N * sizeof(double));
              if (gridit_x == NULL || gridit_y == NULL || gridit_z == NULL)
                {
                  debug_print_malloc_error();
                  error = ERROR_MALLOC;
                  goto cleanup;
                }
            }
          gr_gridit(x_length, x, y, z, PLOT_SURFACE_GRIDIT_N, PLOT_SURFACE_GRIDIT_N, gridit_x, gridit_y, gridit_z);
          gr_surface(PLOT_SURFACE_GRIDIT_N, PLOT_SURFACE_GRIDIT_N, gridit_x, gridit_y, gridit_z,
                     GR_OPTION_COLORED_MESH);
        }
      else
        {
          if (x_length * y_length != z_length)
            {
              error = ERROR_PLOT_COMPONENT_LENGTH_MISMATCH;
              goto cleanup;
            }
          gr_surface(x_length, y_length, x, y, z, GR_OPTION_COLORED_MESH);
        }
      ++current_series;
    }
  plot_draw_axes(subplot_args, 2);
  plot_draw_colorbar(subplot_args, 0.05, 256);

cleanup:
  free(gridit_x);
  free(gridit_y);
  free(gridit_z);

  return error;
}

error_t plot_plot3(grm_args_t *subplot_args)
{
  grm_args_t **current_series;

  args_values(subplot_args, "series", "A", &current_series);
  while (*current_series != NULL)
    {
      double *x, *y, *z;
      unsigned int x_length, y_length, z_length;
      return_error_if(!args_first_value(*current_series, "x", "D", &x, &x_length), ERROR_PLOT_MISSING_DATA);
      return_error_if(!args_first_value(*current_series, "y", "D", &y, &y_length), ERROR_PLOT_MISSING_DATA);
      return_error_if(!args_first_value(*current_series, "z", "D", &z, &z_length), ERROR_PLOT_MISSING_DATA);
      return_error_if(x_length != y_length || x_length != z_length, ERROR_PLOT_COMPONENT_LENGTH_MISMATCH);
      gr_polyline3d(x_length, x, y, z);
      ++current_series;
    }
  plot_draw_axes(subplot_args, 2);

  return NO_ERROR;
}

error_t plot_scatter3(grm_args_t *subplot_args)
{
  grm_args_t **current_series;
  double c_min, c_max;
  unsigned int x_length, y_length, z_length, c_length, i, c_index;
  double *x, *y, *z, *c;
  args_values(subplot_args, "series", "A", &current_series);
  while (*current_series != NULL)
    {
      return_error_if(!args_first_value(*current_series, "x", "D", &x, &x_length), ERROR_PLOT_MISSING_DATA);
      return_error_if(!args_first_value(*current_series, "y", "D", &y, &y_length), ERROR_PLOT_MISSING_DATA);
      return_error_if(!args_first_value(*current_series, "z", "D", &z, &z_length), ERROR_PLOT_MISSING_DATA);
      return_error_if(x_length != y_length || x_length != z_length, ERROR_PLOT_COMPONENT_LENGTH_MISMATCH);
      gr_setmarkertype(GKS_K_MARKERTYPE_SOLID_CIRCLE);
      if (args_first_value(*current_series, "c", "D", &c, &c_length))
        {
          args_values(subplot_args, "crange", "dd", &c_min, &c_max);
          for (i = 0; i < x_length; i++)
            {
              if (i < c_length)
                {
                  c_index = 1000 + (int)(255 * (c[i] - c_min) / c_max);
                }
              else
                {
                  c_index = 989;
                }
              gr_setmarkercolorind(c_index);
              gr_polymarker3d(1, x + i, y + i, z + i);
            }
        }
      else
        {
          if (args_values(*current_series, "c", "i", &c_index))
            {
              gr_setmarkercolorind(c_index);
            }
          gr_polymarker3d(x_length, x, y, z);
        }
      ++current_series;
    }
  plot_draw_axes(subplot_args, 2);

  return NO_ERROR;
}

error_t plot_imshow(grm_args_t *subplot_args)
{
  grm_args_t **current_series;

  args_values(subplot_args, "series", "A", &current_series);
  while (*current_series != NULL)
    {
      /* TODO: Implement me! */
      ++current_series;
    }

  return ERROR_NOT_IMPLEMENTED;
}

error_t plot_isosurface(grm_args_t *subplot_args)
{
  grm_args_t **current_series;

  args_values(subplot_args, "series", "A", &current_series);
  while (*current_series != NULL)
    {
      /* TODO: Implement me! */
      ++current_series;
    }

  return ERROR_NOT_IMPLEMENTED;
}

error_t plot_polar(grm_args_t *subplot_args)
{
  const double *window;
  double r_min, r_max, tick;
  int n;
  grm_args_t **current_series;

  args_values(subplot_args, "window", "D", &window);
  r_min = window[2];
  r_max = window[3];
  tick = 0.5 * gr_tick(r_min, r_max);
  n = (int)ceil((r_max - r_min) / tick);
  r_max = r_min + n * tick;
  args_values(subplot_args, "series", "A", &current_series);
  while (*current_series != NULL)
    {
      double *rho, *theta, *x, *y;
      unsigned int rho_length, theta_length;
      char *spec;
      unsigned int i;
      return_error_if(!args_first_value(*current_series, "x", "D", &theta, &theta_length), ERROR_PLOT_MISSING_DATA);
      return_error_if(!args_first_value(*current_series, "y", "D", &rho, &rho_length), ERROR_PLOT_MISSING_DATA);
      return_error_if(rho_length != theta_length, ERROR_PLOT_COMPONENT_LENGTH_MISMATCH);
      x = malloc(rho_length * sizeof(double));
      y = malloc(rho_length * sizeof(double));
      if (x == NULL || y == NULL)
        {
          debug_print_malloc_error();
          free(x);
          free(y);
          return ERROR_MALLOC;
        }
      for (i = 0; i < rho_length; ++i)
        {
          double current_rho = rho[i] / r_max;
          x[i] = current_rho * cos(theta[i]);
          y[i] = current_rho * sin(theta[i]);
        }
      args_values(*current_series, "spec", "s", &spec); /* `spec` is always set */
      gr_uselinespec(spec);
      gr_polyline(rho_length, x, y);
      free(x);
      free(y);
      ++current_series;
    }

  return NO_ERROR;
}

error_t plot_trisurf(grm_args_t *subplot_args)
{
  grm_args_t **current_series;

  args_values(subplot_args, "series", "A", &current_series);
  while (*current_series != NULL)
    {
      double *x, *y, *z;
      unsigned int x_length, y_length, z_length;
      return_error_if(!args_first_value(*current_series, "x", "D", &x, &x_length), ERROR_PLOT_MISSING_DATA);
      return_error_if(!args_first_value(*current_series, "y", "D", &y, &y_length), ERROR_PLOT_MISSING_DATA);
      return_error_if(!args_first_value(*current_series, "z", "D", &z, &z_length), ERROR_PLOT_MISSING_DATA);
      return_error_if(x_length != y_length || x_length != z_length, ERROR_PLOT_COMPONENT_LENGTH_MISMATCH);
      gr_trisurface(x_length, x, y, z);
      ++current_series;
    }
  plot_draw_axes(subplot_args, 2);
  plot_draw_colorbar(subplot_args, 0.05, 256);

  return NO_ERROR;
}

error_t plot_tricont(grm_args_t *subplot_args)
{
  double z_min, z_max;
  double *levels;
  int num_levels;
  grm_args_t **current_series;
  int i;

  args_values(subplot_args, "zrange", "dd", &z_min, &z_max);
  args_values(subplot_args, "levels", "i", &num_levels);
  levels = malloc(num_levels * sizeof(double));
  if (levels == NULL)
    {
      debug_print_malloc_error();
      return ERROR_MALLOC;
    }
  for (i = 0; i < num_levels; ++i)
    {
      levels[i] = z_min + ((1.0 * i) / (num_levels - 1)) * (z_max - z_min);
    }
  args_values(subplot_args, "series", "A", &current_series);
  while (*current_series != NULL)
    {
      double *x, *y, *z;
      unsigned int x_length, y_length, z_length;
      return_error_if(!args_first_value(*current_series, "x", "D", &x, &x_length), ERROR_PLOT_MISSING_DATA);
      return_error_if(!args_first_value(*current_series, "y", "D", &y, &y_length), ERROR_PLOT_MISSING_DATA);
      return_error_if(!args_first_value(*current_series, "z", "D", &z, &z_length), ERROR_PLOT_MISSING_DATA);
      return_error_if(x_length != y_length || x_length != z_length, ERROR_PLOT_COMPONENT_LENGTH_MISMATCH);
      gr_tricontour(x_length, x, y, z, num_levels, levels);
      ++current_series;
    }
  plot_draw_axes(subplot_args, 2);
  plot_draw_colorbar(subplot_args, 0.05, 256);
  free(levels);

  return NO_ERROR;
}

error_t plot_shade(grm_args_t *subplot_args)
{
  grm_args_t **current_shader;
  const char *data_component_names[] = {"x", "y", NULL};
  double *components[2];
  /* char *spec = ""; TODO: read spec from data! */
  int xform, xbins, ybins;
  double **current_component = components;
  const char **current_component_name = data_component_names;
  unsigned int point_count;

  args_values(subplot_args, "series", "A", &current_shader);
  while (*current_component_name != NULL)
    {
      args_first_value(*current_shader, *current_component_name, "D", current_component, &point_count);
      ++current_component_name;
      ++current_component;
    }
  if (!args_values(subplot_args, "xform", "i", &xform))
    {
      xform = 1;
    }
  if (!args_values(subplot_args, "xbins", "i", &xbins))
    {
      xbins = 100;
    }
  if (!args_values(subplot_args, "ybins", "i", &ybins))
    {
      ybins = 100;
    }
  gr_shadepoints(point_count, components[0], components[1], xform, xbins, ybins);

  return NO_ERROR;
}


/* ~~~~~~~~~~~~~~~~~~~~~~~~~ auxiliary drawing functions ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

error_t plot_draw_axes(grm_args_t *args, unsigned int pass)
{
  const char *kind = NULL;
  const double *viewport, *vp;
  double x_tick;
  double x_org_low, x_org_high;
  int x_major_count;
  double y_tick;
  double y_org_low, y_org_high;
  int y_major_count;
  double z_tick;
  double z_org_low, z_org_high;
  int z_major_count;
  double diag;
  double charheight;
  double ticksize;
  char *title;
  char *x_label, *y_label, *z_label;

  args_values(args, "kind", "s", &kind);
  args_values(args, "viewport", "D", &viewport);
  args_values(args, "vp", "D", &vp);
  args_values(args, "xtick", "d", &x_tick);
  args_values(args, "xorg", "dd", &x_org_low, &x_org_high);
  args_values(args, "xmajor", "i", &x_major_count);
  args_values(args, "ytick", "d", &y_tick);
  args_values(args, "yorg", "dd", &y_org_low, &y_org_high);
  args_values(args, "ymajor", "i", &y_major_count);

  gr_setlinecolorind(1);
  gr_setlinewidth(1);
  diag = sqrt((viewport[1] - viewport[0]) * (viewport[1] - viewport[0]) +
              (viewport[3] - viewport[2]) * (viewport[3] - viewport[2]));
  charheight = max(0.018 * diag, 0.012);
  gr_setcharheight(charheight);
  ticksize = 0.0075 * diag;
  if (str_equals_any(kind, 5, "wireframe", "surface", "plot3", "scatter3", "trisurf"))
    {
      args_values(args, "ztick", "d", &z_tick);
      args_values(args, "zorg", "dd", &z_org_low, &z_org_high);
      args_values(args, "zmajor", "i", &z_major_count);
      if (pass == 1)
        {
          gr_grid3d(x_tick, 0, z_tick, x_org_low, y_org_high, z_org_low, 2, 0, 2);
          gr_grid3d(0, y_tick, 0, x_org_low, y_org_high, z_org_low, 0, 2, 0);
        }
      else
        {
          gr_axes3d(x_tick, 0, z_tick, x_org_low, y_org_low, z_org_low, x_major_count, 0, z_major_count, -ticksize);
          gr_axes3d(0, y_tick, 0, x_org_high, y_org_low, z_org_low, 0, y_major_count, 0, ticksize);
        }
    }
  else
    {
      if (str_equals_any(kind, 3, "heatmap", "shade", "nonuniformheatmap"))
        {
          ticksize = -ticksize;
        }
      if (!str_equals_any(kind, 1, "shade"))
        {
          gr_grid(x_tick, y_tick, 0, 0, x_major_count, y_major_count);
        }
      gr_axes(x_tick, y_tick, x_org_low, y_org_low, x_major_count, y_major_count, ticksize);
      gr_axes(x_tick, y_tick, x_org_high, y_org_high, -x_major_count, -y_major_count, -ticksize);
    }

  if (args_values(args, "title", "s", &title))
    {
      gr_savestate();
      gr_settextalign(GKS_K_TEXT_HALIGN_CENTER, GKS_K_TEXT_VALIGN_TOP);
      gr_textext(0.5 * (viewport[0] + viewport[1]), vp[3], title);
      gr_restorestate();
    }

  if (str_equals_any(kind, 5, "wireframe", "surface", "plot3", "scatter3", "trisurf"))
    {
      if (args_values(args, "xlabel", "s", &x_label) && args_values(args, "ylabel", "s", &y_label) &&
          args_values(args, "zlabel", "s", &z_label))
        {
          gr_titles3d(x_label, y_label, z_label);
        }
    }
  else
    {
      if (args_values(args, "xlabel", "s", &x_label))
        {
          gr_savestate();
          gr_settextalign(GKS_K_TEXT_HALIGN_CENTER, GKS_K_TEXT_VALIGN_BOTTOM);
          gr_textext(0.5 * (viewport[0] + viewport[1]), vp[2] + 0.5 * charheight, x_label);
          gr_restorestate();
        }
      if (args_values(args, "ylabel", "s", &y_label))
        {
          gr_savestate();
          gr_settextalign(GKS_K_TEXT_HALIGN_CENTER, GKS_K_TEXT_VALIGN_TOP);
          gr_setcharup(-1, 0);
          gr_textext(vp[0] + 0.5 * charheight, 0.5 * (viewport[2] + viewport[3]), y_label);
          gr_restorestate();
        }
    }

  return NO_ERROR;
}

error_t plot_draw_polar_axes(grm_args_t *args)
{
  const double *window, *viewport;
  double diag;
  double charheight;
  double r_min, r_max;
  double tick;
  double x[2], y[2];
  int i, n, alpha;
  char text_buffer[PLOT_POLAR_AXES_TEXT_BUFFER];

  args_values(args, "viewport", "D", &viewport);
  diag = sqrt((viewport[1] - viewport[0]) * (viewport[1] - viewport[0]) +
              (viewport[3] - viewport[2]) * (viewport[3] - viewport[2]));
  charheight = max(0.018 * diag, 0.012);

  args_values(args, "window", "D", &window);
  r_min = window[2];
  r_max = window[3];

  gr_savestate();
  gr_setcharheight(charheight);
  gr_setlinetype(GKS_K_LINETYPE_SOLID);

  tick = 0.5 * gr_tick(r_min, r_max);
  n = (int)ceil((r_max - r_min) / tick);
  for (i = 0; i <= n; i++)
    {
      double r = (i * 1.0) / n;
      if (i % 2 == 0)
        {
          gr_setlinecolorind(88);
          if (i > 0)
            {
              gr_drawarc(-r, r, -r, r, 0, 180);
              gr_drawarc(-r, r, -r, r, 180, 360);
            }
          gr_settextalign(GKS_K_TEXT_HALIGN_LEFT, GKS_K_TEXT_VALIGN_HALF);
          x[0] = 0.05;
          y[0] = r;
          gr_wctondc(x, y);
          snprintf(text_buffer, PLOT_POLAR_AXES_TEXT_BUFFER, "%g", r_min + i * tick);
          gr_text(x[0], y[0], text_buffer);
        }
      else
        {
          gr_setlinecolorind(90);
          gr_drawarc(-r, r, -r, r, 0, 180);
          gr_drawarc(-r, r, -r, r, 180, 360);
        }
    }
  for (alpha = 0; alpha < 360; alpha += 45)
    {
      x[0] = cos(alpha * M_PI / 180.0);
      y[0] = sin(alpha * M_PI / 180.0);
      x[1] = 0.0;
      y[1] = 0.0;
      gr_polyline(2, x, y);
      gr_settextalign(GKS_K_TEXT_HALIGN_CENTER, GKS_K_TEXT_VALIGN_HALF);
      x[0] *= 1.1;
      y[0] *= 1.1;
      gr_wctondc(x, y);
      snprintf(text_buffer, PLOT_POLAR_AXES_TEXT_BUFFER, "%d\xb0", alpha);
      gr_textext(x[0], y[0], text_buffer);
    }
  gr_restorestate();

  return NO_ERROR;
}

error_t plot_draw_legend(grm_args_t *subplot_args)
{
  const char **labels, **current_label;
  unsigned int num_labels, num_series;
  grm_args_t **current_series;
  const double *viewport;
  int location;
  double px, py, w, h;
  double tbx[4], tby[4];
  double legend_symbol_x[2], legend_symbol_y[2];


  return_error_if(!args_first_value(subplot_args, "labels", "S", &labels, &num_labels), ERROR_PLOT_MISSING_LABELS);
  logger((stderr, "Draw a legend with %d labels\n", num_labels));
  args_first_value(subplot_args, "series", "A", &current_series, &num_series);
  args_values(subplot_args, "viewport", "D", &viewport);
  args_values(subplot_args, "location", "i", &location);
  gr_savestate();
  gr_selntran(0);
  gr_setscale(0);
  w = 0;
  for (current_label = labels; *current_label != NULL; ++current_label)
    {
      gr_inqtextext(0, 0, *(char **)current_label, tbx, tby);
      w = max(w, tbx[2]);
    }

  h = (num_series + 1) * 0.03;
  if (int_equals_any(location, 3, 8, 9, 10))
    {
      px = 0.5 * (viewport[0] + viewport[1] - w);
    }
  else if (int_equals_any(location, 3, 2, 3, 6))
    {
      px = viewport[0] + 0.11;
    }
  else
    {
      px = viewport[1] - 0.05 - w;
    }
  if (int_equals_any(location, 4, 5, 6, 7, 10))
    {
      py = 0.5 * (viewport[2] + viewport[3] + h) - 0.03;
    }
  else if (int_equals_any(location, 3, 3, 4, 8))
    {
      py = viewport[2] + h;
    }
  else
    {
      py = viewport[3] - 0.06;
    }

  gr_setfillintstyle(GKS_K_INTSTYLE_SOLID);
  gr_setfillcolorind(0);
  gr_fillrect(px - 0.08, px + w + 0.02, py + 0.03, py - 0.03 * num_series);
  gr_setlinetype(GKS_K_INTSTYLE_SOLID);
  gr_setlinecolorind(1);
  gr_setlinewidth(1);
  gr_drawrect(px - 0.08, px + w + 0.02, py + 0.03, py - 0.03 * num_series);
  gr_uselinespec(" ");
  current_label = labels;
  while (*current_series != NULL)
    {
      char *spec;
      int mask;

      gr_savestate();
      args_values(*current_series, "spec", "s", &spec); /* `spec` is always set */
      mask = gr_uselinespec(spec);
      if (int_equals_any(mask, 5, 0, 1, 3, 4, 5))
        {
          legend_symbol_x[0] = px - 0.07;
          legend_symbol_x[1] = px - 0.01;
          legend_symbol_y[0] = py;
          legend_symbol_y[1] = py;
          gr_polyline(2, legend_symbol_x, legend_symbol_y);
        }
      if (mask & 2)
        {
          legend_symbol_x[0] = px - 0.06;
          legend_symbol_x[1] = px - 0.02;
          legend_symbol_y[0] = py;
          legend_symbol_y[1] = py;
          gr_polymarker(2, legend_symbol_x, legend_symbol_y);
        }
      gr_restorestate();
      gr_settextalign(GKS_K_TEXT_HALIGN_LEFT, GKS_K_TEXT_VALIGN_HALF);
      if (*current_label != NULL)
        {
          gr_textext(px, py, (char *)*current_label);
          ++current_label;
        }
      py -= 0.03;
      ++current_series;
    }
  gr_selntran(1);
  gr_restorestate();

  return NO_ERROR;
}

error_t plot_draw_colorbar(grm_args_t *args, double off, unsigned int colors)
{
  const double *viewport;
  double c_min, c_max;
  int *data;
  double diag, charheight;
  int scale, flip, options;
  unsigned int i;

  gr_savestate();
  args_values(args, "viewport", "D", &viewport);
  if (!args_values(args, "clim", "dd", &c_min, &c_max))
    {
      args_values(args, "zrange", "dd", &c_min, &c_max);
    }
  data = malloc(colors * sizeof(int));
  if (data == NULL)
    {
      debug_print_malloc_error();
      return ERROR_MALLOC;
    }
  for (i = 0; i < colors; ++i)
    {
      data[i] = 1000 + 255 * i / (colors - 1);
    }
  gr_inqscale(&options);
  if (args_values(args, "xflip", "i", &flip) && flip)
    {
      options = (options | GR_OPTION_FLIP_Y) & ~GR_OPTION_FLIP_X;
      gr_setscale(options);
    }
  else if (args_values(args, "yflip", "i", &flip) && flip)
    {
      options = options & ~GR_OPTION_FLIP_Y & ~GR_OPTION_FLIP_X;
      gr_setscale(options);
    }
  else
    {
      options = options & ~GR_OPTION_FLIP_X;
      gr_setscale(options);
    }
  gr_setwindow(0.0, 1.0, c_min, c_max);
  gr_setviewport(viewport[1] + 0.02 + off, viewport[1] + 0.05 + off, viewport[2], viewport[3]);
  gr_cellarray(0, 1, c_max, c_min, 1, colors, 1, 1, 1, colors, data);
  diag = sqrt((viewport[1] - viewport[0]) * (viewport[1] - viewport[0]) +
              (viewport[3] - viewport[2]) * (viewport[3] - viewport[2]));
  charheight = max(0.016 * diag, 0.012);
  gr_setcharheight(charheight);
  args_values(args, "scale", "i", &scale);
  if (scale & GR_OPTION_Z_LOG)
    {
      gr_setscale(GR_OPTION_Y_LOG);
      gr_axes(0, 2, 1, c_min, 0, 1, 0.005);
    }
  else
    {
      double c_tick = 0.5 * gr_tick(c_min, c_max);
      gr_axes(0, c_tick, 1, c_min, 0, 1, 0.005);
    }
  free(data);
  gr_restorestate();

  return NO_ERROR;
}


/* ~~~~~~~~~~~~~~~~~~~~~~~~~ util ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

double find_max_step(unsigned int n, const double *x)
{
  double max_step = 0.0;
  unsigned int i;

  if (n < 2)
    {
      return 0.0;
    }
  for (i = 1; i < n; ++i)
    {
      max_step = max(x[i] - x[i - 1], max_step);
    }

  return max_step;
}

const char *next_fmt_key(const char *kind)
{
  static const char *saved_fmt = NULL;
  static char fmt_key[2] = {0, 0};

  if (kind != NULL)
    {
      string_map_at(fmt_map, kind, (char **)&saved_fmt);
    }
  if (saved_fmt == NULL)
    {
      return NULL;
    }
  fmt_key[0] = *saved_fmt;
  if (*saved_fmt != '\0')
    {
      ++saved_fmt;
    }

  return fmt_key;
}

int get_id_from_args(const grm_args_t *args, int *plot_id, int *subplot_id, int *series_id)
{
  const char *combined_id;
  int _plot_id = -1, _subplot_id = 0, _series_id = 0;

  if (args_values(args, "id", "s", &combined_id))
    {
      const char *valid_id_delims = ":.";
      int *id_ptrs[4], **current_id_ptr;
      char *copied_id_str, *current_id_str;
      size_t segment_length;
      int is_last_segment;

      id_ptrs[0] = &_plot_id;
      id_ptrs[1] = &_subplot_id;
      id_ptrs[2] = &_series_id;
      id_ptrs[3] = NULL;
      if ((copied_id_str = gks_strdup(combined_id)) == NULL)
        {
          debug_print_malloc_error();
          return 0;
        }

      current_id_ptr = id_ptrs;
      current_id_str = copied_id_str;
      is_last_segment = 0;
      while (*current_id_ptr != NULL && !is_last_segment)
        {
          segment_length = strcspn(current_id_str, valid_id_delims);
          if (current_id_str[segment_length] == '\0')
            {
              is_last_segment = 1;
            }
          else
            {
              current_id_str[segment_length] = '\0';
            }
          if (*current_id_str != '\0')
            {
              if (!str_to_uint(current_id_str, (unsigned int *)*current_id_ptr))
                {
                  logger((stderr, "Got an invalid id \"%s\"\n", current_id_str));
                }
              else
                {
                  logger((stderr, "Read id: %d\n", **current_id_ptr));
                }
            }
          ++current_id_ptr;
          ++current_id_str;
        }

      free(copied_id_str);
    }
  else
    {
      args_values(args, "plot_id", "i", &_plot_id);
      args_values(args, "subplot_id", "i", &_subplot_id);
      args_values(args, "series_id", "i", &_series_id);
    }
  /* plot id `0` references the first plot object (implicit object) -> handle it as the first plot and shift all ids by
   * one */
  *plot_id = _plot_id + 1;
  *subplot_id = _subplot_id;
  *series_id = _series_id;

  return _plot_id > 0 || _subplot_id > 0 || _series_id > 0;
}

int get_figure_size(const grm_args_t *plot_args, int *pixel_width, int *pixel_height, double *metric_width,
                    double *metric_height)
{
  double display_metric_width, display_metric_height;
  int display_pixel_width, display_pixel_height;
  double dpm[2], dpi[2];
  int tmp_size_i[2], pixel_size[2];
  double tmp_size_d[2], metric_size[2];
  int i;

  if (plot_args == NULL)
    {
      plot_args = active_plot_args;
    }

#ifdef __EMSCRIPTEN__
  display_metric_width = 0.16384;
  display_metric_height = 0.12288;
  display_pixel_width = 640;
  display_pixel_height = 480;
#else
  gr_inqdspsize(&display_metric_width, &display_metric_height, &display_pixel_width, &display_pixel_height);
#endif
  dpm[0] = display_pixel_width / display_metric_width;
  dpm[1] = display_pixel_height / display_metric_height;
  dpi[0] = dpm[0] * 0.0254;
  dpi[1] = dpm[1] * 0.0254;

  /* TODO: Overwork this calculation */
  if (args_values(plot_args, "figsize", "dd", &tmp_size_d[0], &tmp_size_d[1]))
    {
      for (i = 0; i < 2; ++i)
        {
          pixel_size[i] = round(tmp_size_d[i] * dpi[i]);
          metric_size[i] = tmp_size_d[i] / 0.0254;
        }
    }
  else if (args_values(plot_args, "size", "dd", &tmp_size_d[0], &tmp_size_d[1]))
    {
      if (dpi[0] > 300 || dpi[1] > 300)
        {
          for (i = 0; i < 2; ++i)
            {
              pixel_size[i] = round(tmp_size_d[i] * dpi[i] / 100.0);
              metric_size[i] = tmp_size_d[i] / 0.000254;
            }
        }
      else
        {
          for (i = 0; i < 2; ++i)
            {
              pixel_size[i] = round(tmp_size_d[i]);
              metric_size[i] = tmp_size_d[i] / dpm[i];
            }
        }
    }
  else if (args_values(plot_args, "size", "ii", &tmp_size_i[0], &tmp_size_i[1]))
    {
      for (i = 0; i < 2; ++i)
        {
          pixel_size[i] = tmp_size_i[i];
          metric_size[i] = tmp_size_i[i] / dpm[i];
        }
    }
  else
    {
      /* If this branch is executed, there is an internal error (size has a default value if not set by the user) */
      return 0;
    }

  logger((stderr, "figure pixel size: (%d, %d)\n", pixel_size[0], pixel_size[1]));
  logger((stderr, "device dpi: (%lf, %lf)\n", dpi[0], dpi[1]));

  if (pixel_width != NULL)
    {
      *pixel_width = pixel_size[0];
    }
  if (pixel_height != NULL)
    {
      *pixel_height = pixel_size[1];
    }
  if (metric_width != NULL)
    {
      *metric_width = metric_size[0];
    }
  if (metric_height != NULL)
    {
      *metric_height = metric_size[1];
    }

  return 1;
}

int get_focus_and_factor(const int x1, const int y1, const int x2, const int y2, const int keep_aspect_ratio,
                         double *factor_x, double *factor_y, double *focus_x, double *focus_y,
                         grm_args_t **subplot_args)
{
  double ndc_box_x[4], ndc_box_y[4];
  double ndc_left, ndc_top, ndc_right, ndc_bottom;
  const double *wswindow, *viewport;
  int width, height, max_width_height;

  get_figure_size(NULL, &width, &height, NULL, NULL);
  max_width_height = max(width, height);

  if (x1 <= x2)
    {
      ndc_left = (double)x1 / max_width_height;
      ndc_right = (double)x2 / max_width_height;
    }
  else
    {
      ndc_left = (double)x2 / max_width_height;
      ndc_right = (double)x1 / max_width_height;
    }
  if (y1 <= y2)
    {
      ndc_top = (double)(height - y1) / max_width_height;
      ndc_bottom = (double)(height - y2) / max_width_height;
    }
  else
    {
      ndc_top = (double)(height - y2) / max_width_height;
      ndc_bottom = (double)(height - y1) / max_width_height;
    }

  ndc_box_x[0] = ndc_left;
  ndc_box_y[0] = ndc_bottom;
  ndc_box_x[1] = ndc_right;
  ndc_box_y[1] = ndc_bottom;
  ndc_box_x[2] = ndc_left;
  ndc_box_y[2] = ndc_top;
  ndc_box_x[3] = ndc_right;
  ndc_box_y[3] = ndc_top;
  *subplot_args = get_subplot_from_ndc_points(array_size(ndc_box_x), ndc_box_x, ndc_box_y);
  if (*subplot_args == NULL)
    {
      return 0;
    }
  args_values(*subplot_args, "viewport", "D", &viewport);
  args_values(active_plot_args, "wswindow", "D", &wswindow);

  *factor_x = abs(x1 - x2) / (width * (viewport[1] - viewport[0]) / (wswindow[1] - wswindow[0]));
  *factor_y = abs(y1 - y2) / (height * (viewport[3] - viewport[2]) / (wswindow[3] - wswindow[2]));
  if (keep_aspect_ratio)
    {
      if (*factor_x <= *factor_y)
        {
          *factor_x = *factor_y;
          if (x1 > x2)
            {
              ndc_left = ndc_right - *factor_x * (viewport[1] - viewport[0]);
            }
        }
      else
        {
          *factor_y = *factor_x;
          if (y1 > y2)
            {
              ndc_top = ndc_bottom + *factor_y * (viewport[3] - viewport[2]);
            }
        }
    }
  *focus_x = (ndc_left - *factor_x * viewport[0]) / (1 - *factor_x) - (viewport[0] + viewport[1]) / 2.0;
  *focus_y = (ndc_top - *factor_y * viewport[3]) / (1 - *factor_y) - (viewport[2] + viewport[3]) / 2.0;
  return 1;
}
grm_args_t *get_subplot_from_ndc_point(double x, double y)
{
  grm_args_t **subplot_args;
  const double *viewport;

  args_values(active_plot_args, "subplots", "A", &subplot_args);
  while (*subplot_args != NULL)
    {
      if (args_values(*subplot_args, "viewport", "D", &viewport))
        {
          if (viewport[0] <= x && x <= viewport[1] && viewport[2] <= y && y <= viewport[3])
            {
              unsigned int array_index;
              args_values(*subplot_args, "array_index", "i", &array_index);
              logger((stderr, "Found subplot id \"%u\" for ndc point (%lf, %lf)\n", array_index + 1, x, y));

              return *subplot_args;
            }
        }
      ++subplot_args;
    }

  return NULL;
}

grm_args_t *get_subplot_from_ndc_points(unsigned int n, const double *x, const double *y)
{
  grm_args_t *subplot_args;
  unsigned int i;

  for (i = 0, subplot_args = NULL; i < n && subplot_args == NULL; ++i)
    {
      subplot_args = get_subplot_from_ndc_point(x[i], y[i]);
    }

  return subplot_args;
}


/* ========================= methods ================================================================================ */

/* ------------------------- args set ------------------------------------------------------------------------------- */

DEFINE_SET_METHODS(args)

int args_set_entry_copy(args_set_entry_t *copy, args_set_const_entry_t entry)
{
  /* discard const because it is necessary to work on the object itself */
  /* TODO create two set types: copy and pointer version */
  *copy = (args_set_entry_t)entry;
  return 1;
}

void args_set_entry_delete(args_set_entry_t entry UNUSED) {}

size_t args_set_entry_hash(args_set_const_entry_t entry)
{
  return (size_t)entry;
}

int args_set_entry_equals(args_set_const_entry_t entry1, args_set_const_entry_t entry2)
{
  return entry1 == entry2;
}


/* ------------------------- string-to-plot_func map ---------------------------------------------------------------- */

DEFINE_MAP_METHODS(plot_func)

int plot_func_map_value_copy(plot_func_t *copy, const plot_func_t value)
{
  *copy = value;

  return 1;
}

void plot_func_map_value_delete(plot_func_t value UNUSED) {}


/* ------------------------- string-to-args_set map ----------------------------------------------------------------- */

DEFINE_MAP_METHODS(args_set)

int args_set_map_value_copy(args_set_t **copy, const args_set_t *value)
{
  /* discard const because it is necessary to work on the object itself */
  /* TODO create two map types: copy and pointer version */
  *copy = (args_set_t *)value;

  return 1;
}

void args_set_map_value_delete(args_set_t *value UNUSED) {}


#undef DEFINE_SET_METHODS
#undef DEFINE_MAP_METHODS


/* ######################### public implementation ################################################################## */

/* ========================= functions ============================================================================== */

/* ------------------------- plot ----------------------------------------------------------------------------------- */

void grm_finalize(void)
{
  if (plot_static_variables_initialized)
    {
      grm_args_delete(global_root_args);
      global_root_args = NULL;
      active_plot_args = NULL;
      active_plot_index = 0;
      event_queue_delete(event_queue);
      event_queue = NULL;
      string_map_delete(fmt_map);
      fmt_map = NULL;
      plot_func_map_delete(plot_func_map);
      plot_func_map = NULL;
      string_map_delete(plot_valid_keys_map);
      plot_valid_keys_map = NULL;
      plot_static_variables_initialized = 0;
    }
}

int grm_clear(void)
{
  if (plot_init_static_variables() != NO_ERROR)
    {
      return 0;
    }
  grm_args_clear(active_plot_args);
  if (plot_init_args_structure(active_plot_args, plot_hierarchy_names + 1, 1) != NO_ERROR)
    {
      return 0;
    }

  return 1;
}

unsigned int grm_max_plotid(void)
{
  unsigned int args_array_length = 0;

  if (args_first_value(global_root_args, "plots", "A", NULL, &args_array_length))
    {
      --args_array_length;
    }

  return args_array_length;
}

int grm_merge(const grm_args_t *args)
{
  return grm_merge_extended(args, 0, NULL);
}

int grm_merge_extended(const grm_args_t *args, int hold, const char *identificator)
{
  if (plot_init_static_variables() != NO_ERROR)
    {
      return 0;
    }
  if (args != NULL)
    {
      if (plot_merge_args(global_root_args, args, NULL, NULL, hold) != NO_ERROR)
        {
          return 0;
        }
    }

  process_events();
  event_queue_enqueue_merge_end_event(event_queue, identificator);
  process_events();

  return 1;
}

int grm_merge_hold(const grm_args_t *args)
{
  return grm_merge_extended(args, 1, NULL);
}

int grm_merge_named(const grm_args_t *args, const char *identificator)
{
  return grm_merge_extended(args, 0, identificator);
}

int grm_plot(const grm_args_t *args)
{
  grm_args_t **current_subplot_args;
  plot_func_t plot_func;
  const char *kind = NULL;
  if (!grm_merge(args))
    {
      return 0;
    }

  plot_set_attribute_defaults(active_plot_args);
  plot_pre_plot(active_plot_args);
  args_values(active_plot_args, "subplots", "A", &current_subplot_args);
  while (*current_subplot_args != NULL)
    {
      plot_pre_subplot(*current_subplot_args);
      args_values(*current_subplot_args, "kind", "s", &kind);
      logger((stderr, "Got keyword \"kind\" with value \"%s\"\n", kind));
      if (!plot_func_map_at(plot_func_map, kind, &plot_func))
        {
          return 0;
        }
      if (plot_func(*current_subplot_args) != NO_ERROR)
        {
          return 0;
        };
      plot_post_subplot(*current_subplot_args);
      ++current_subplot_args;
    }
  plot_post_plot(active_plot_args);

  process_events();

#ifndef NDEBUG
  logger((stderr, "root args after \"grm_plot\" (active_plot_index: %d):\n", active_plot_index - 1));
  grm_dump(global_root_args, stderr);
#endif

  return 1;
}

int grm_switch(unsigned int id)
{
  grm_args_t **args_array = NULL;
  unsigned int args_array_length = 0;

  if (plot_init_static_variables() != NO_ERROR)
    {
      return 0;
    }

  if (plot_init_args_structure(global_root_args, plot_hierarchy_names, id + 1) != NO_ERROR)
    {
      return 0;
    }
  if (!args_first_value(global_root_args, "plots", "A", &args_array, &args_array_length))
    {
      return 0;
    }
  if (id + 1 > args_array_length)
    {
      return 0;
    }

  active_plot_index = id + 1;
  active_plot_args = args_array[id];

  return 1;
}
