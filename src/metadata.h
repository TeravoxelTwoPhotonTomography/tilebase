/**
 * \file
 * Interface for reading and writing tile metadata.
 */
#ifdef __cplusplus
extern "C" {
#endif

typedef struct _metadata_t*     metadata_t;
typedef struct _metadata_api_t  metadata_api_t;

unsigned   MetadataIsFound(const char* tilepath);
metadata_t MetadataOpen(const char *tilepath, const char *format, const char *mode);
void       MetadataClose(metadata_t self);

void*      MetadataContext(metadata_t self);
char*      MetadataError(metadata_t self);

ndio_t     MetadataOpenVolume(metadata_t self, const char* mode);

unsigned   MetadataGetTileAABB(metadata_t self, tile_t tile);
unsigned   MetadataSetTileAABB(metadata_t self, tile_t tile);

/// \todo MetadataSetFormat - set the tile format string 
/// \todo MetadataCopy

#ifdef __cplusplus
} //extern "C"
#endif