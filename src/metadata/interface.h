/** \file
 *  Interface for reading/writing metadata for a tile.
 *
 *  To enable reading/writing from a custom tile layout:
 *  1. Implementing the functions in this interface.
 *  2. Include the appropriate get_metadata_api_t function in metadata.c \see static_format_loaders.
 */
#pragma once
#ifdef __cplusplus
extern"C"{
#endif

// NOTE: include "metadata.h" before this file to define these types.
//typedef struct _metadata_t*     metadata_t;
//typedef struct _metadata_api_t  metadata_api_t;

/// \returns a null-terminated string reflecting the name of the implementation used to read/write a tile's metadata.
typedef const char* (*_metadata_name_t)      ();
/// \returns 1 if the tile metadata at \a path can be opened with mode \a mode, otherwise 0.
typedef unsigned    (*_metadata_is_fmt_t)    (const char* path, const char* mode);
/// \returns a null-terminated string reflecting the name of the implementation used to read/write a tile's metadata.
typedef void*       (*_metadata_open_t)      (const char* path, const char* mode);
/// Closes an open metadata context.
typedef void        (*_metadata_close_t)     (metadata_t self);
/**
 * Get the origin of the tile in nanometers (nm).
 *
 * Implementations can assume <tt>nelem!=NULL</tt>.
 *
 * If <tt>origin==NULL</tt>, the function should return 0, and set <tt>*nelem</tt>
 * to the number of elements required for the <tt>*origin</tt> array.
 *
 * \param[in]   self    The metadata context.
 * \param[out]  nelem   The number of elements returned in the array pointed to by \a origin.
 * \param[out]  origin  An array containing the tile origin in global coordinates.  The array
 *                      will be allocated by the caller.
 */
typedef unsigned    (*_metadata_origin_t)    (metadata_t self, size_t *nelem, int64_t*  origin);
/// Set the origin of the tile in nanometers (nm).
typedef unsigned    (*_metadata_set_origin_t)(metadata_t self, size_t  nelem, int64_t*  origin);
/**
 * Get the size of the tile's bounding box in nanometers (nm).
 *
 * Implementations can assume <tt>nelem!=NULL</tt>.
 *
 * If <tt>shape==NULL</tt>, the function should return 0, and set <tt>*nelem</tt>
 * to the number of elements required for the <tt>*shape</tt> array.
 *
 * \param[in]   self    The metadata context.
 * \param[out]  nelem   The number of elements returned in the array pointed to by \a shape.
 * \param[out]  shape   An array containing the tile shape in global coordinates.  The array
 *                      will be allocated by the caller.
 */
typedef unsigned    (*_metadata_shape_t)     (metadata_t self, size_t *nelem, int64_t*  shape);
/// Set the size of the tile's bounding box in nanometers (nm).
typedef unsigned    (*_metadata_set_shape_t) (metadata_t self, size_t  nelem, int64_t*  shape);
/**
 * Opens the file containing the volume according to the specified \a mode.
 */
typedef ndio_t      (*_metadata_get_vol_t)   (metadata_t self,const char *mode);
/**
 * Tilebase metadata formats must implement the functions in this interface.
 */
struct _metadata_api_t
{
  _metadata_name_t       name;      ///< \returns a null-terminated string reflecting the name of the implementation used to read/write a tile's metadata.
  _metadata_is_fmt_t     is_fmt;    ///< \returns 1 if the tile metadata at \a path can be opened with mode \a mode, otherwise 0.
  _metadata_open_t       open;      ///< \returns NULL on error, otherwise a pointer to a context for reading or writing metadata. \param[in] path Path to a tile directory.  \param[in] mode Should be "r" for reading, or "w" for writing.
  _metadata_close_t      close;     ///< Closes an open metadata context.
  _metadata_origin_t     origin;    ///< Get the origin of the tile in nanometers (nm).
  _metadata_set_origin_t set_origin;///< Set the origin of the tile in nanometers (nm).
  _metadata_shape_t      shape;     ///< Get the size of the tile's bounding box in nanometers (nm).
  _metadata_set_shape_t  set_shape; ///< Set the size of the tile's bounding box in nanometers (nm).
  _metadata_get_vol_t    get_vol;   ///< Open the file for volume \a i according to the specified \a mode.
  void *lib;                        ///< Handle to the library context (if not null)
};
typedef const metadata_api_t* (*get_metadata_api_t)(void); ///< \returns the interface used to read/write tile metadata.  The caller will not free the returned pointer.  It should be statically allocated by the implementation.
#ifdef __cplusplus
}//extern "C"{
#endif