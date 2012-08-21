/**
 * \file
 *  Loading ndio format plugins.
 * \ingroup ndioplugins
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct _metadata_api_t** metadata_apis_t;                    ///< Type that describes a buffer containing loaded plugin's. \ingroup metadataplugins

  metadata_apis_t MetadataLoadPlugins(const char *path, size_t *n);    // Loads the plugins contained in \a path.  \returns 0 on failure, otherwise an array with the loaded plugins. \param[out] n The number of loaded plugins.
  void            MetadataFreePlugins(metadata_apis_t fmts, size_t n); // Releases resources.  Always succeeds.

#ifdef __cplusplus
}//extern "C" {
#endif
