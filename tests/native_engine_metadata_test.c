#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <platform/native_engine_metadata.h>

int main(void)
{
    char retail[20];
    for (int profile = 0; profile < NATIVE_ENGINE_COUNT; profile++)
    {
        assert(NativeEngineMetadata_DecodeWord(NativeEngineMetadata_EncodeWord(profile)) == profile);
        memset(retail, 0xcc, sizeof(retail));
        NativeEngineMetadata_StoreRetail(retail, profile);
        assert(NativeEngineMetadata_LoadRetail(retail) == profile);
        for (int i = 5; i < 20; i++) assert(retail[i] == 0);
    }
    assert(NativeEngineMetadata_DecodeWord(0) == NATIVE_ENGINE_DEFAULT);
    assert(NativeEngineMetadata_DecodeWord(0xdeadbeef) == NATIVE_ENGINE_DEFAULT);
    assert(NativeEngineMetadata_DecodeWord(NATIVE_ENGINE_METADATA_TAG | NATIVE_ENGINE_COUNT) == NATIVE_ENGINE_METADATA_INVALID);
    assert(NativeEngineMetadata_DecodeWord(NATIVE_ENGINE_METADATA_TAG | 255) == NATIVE_ENGINE_METADATA_INVALID);
    assert(NativeEngineMetadata_EncodeWord(-1) == 0);
    assert(NativeEngineMetadata_EncodeWord(NATIVE_ENGINE_COUNT) == 0);
    memset(retail, 0, sizeof(retail));
    assert(NativeEngineMetadata_LoadRetail(retail) == NATIVE_ENGINE_DEFAULT);
    memset(retail, 0xcc, sizeof(retail));
    assert(NativeEngineMetadata_LoadRetail(retail) == NATIVE_ENGINE_DEFAULT);
    NativeEngineMetadata_StoreRetail(retail, NATIVE_ENGINE_BALANCED);
    retail[4] = (char)255;
    assert(NativeEngineMetadata_LoadRetail(retail) == NATIVE_ENGINE_METADATA_INVALID);
    retail[3] = 2;
    assert(NativeEngineMetadata_LoadRetail(retail) == NATIVE_ENGINE_DEFAULT);
    NativeEngineMetadata_StoreRetail(retail, -1);
    assert(NativeEngineMetadata_LoadRetail(retail) == NATIVE_ENGINE_DEFAULT);
    puts("native engine metadata tests passed");
    return 0;
}
