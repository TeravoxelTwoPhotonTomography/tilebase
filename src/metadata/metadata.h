/**
 * \file
 * Interface for reading and writing tile metadata.
 *
 * \todo add MetadataAddPluginPath() and corresponding functionality for
 *       search paths to metadata/plugin.c. see ndioAddPluginPath() for reference.
 *       This would enable plugin specific tests to run in the build tree.
 */
#ifdef __cplusplus
extern "C" {
#endif

typedef struct _metadata_t*     metadata_t;
typedef struct _metadata_api_t  metadata_api_t;

unsigned    MetadataIsFound(const char* tilepath);
metadata_t  MetadataOpen(const char *tilepath, const char *format, const char *mode);
void        MetadataClose(metadata_t self);

unsigned    MetadataFormatCount();
const char* MetadataFormatName(unsigned i);

void*       MetadataContext(metadata_t self);
char*       MetadataError(metadata_t self);

ndio_t      MetadataOpenVolume(metadata_t self, const char* mode);

/// \todo Metadata interface should not use tile objects, use aabb instead
unsigned    MetadataGetTileAABB(metadata_t self, tile_t tile);
unsigned    MetadataSetTileAABB(metadata_t self, tile_t tile);

unsigned    MetadataGetTransform(metadata_t self, float *transform);

/// \todo MetadataSetFormat - set the tile format string 
/// \todo MetadataCopy

#ifdef __cplusplus
} //extern "C"
#endif