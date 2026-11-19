#include "texture_system.h"

#include "core/logger.h"
#include "core/kmemory.h"
#include "core/kstring.h"

#include "containers/hashtable.h"
#include "renderer/renderer_frontend.h"

// TODO: resource loader
#define STB_IMAGE_IMPLEMENTATION
#include "vendor/stb_image.h"

typedef struct texture_system_state {
    // hashtable texture_table;
    // u32 max_texture_count;

    texture_system_config config;
    texture default_texture;

    // array of registered textures
    texture* registered_textures;

    // hashtable for textures lookup
    hashtable registered_texture_table;
} texture_system_state;

typedef struct texture_reference{
    u64 reference_count;
    u32 handle;
    b8 auto_release;
}texture_reference;

static texture_system_state* state_ptr = 0;

b8 create_default_textures(texture_system_state* state);
void destroy_default_textures(texture_system_state* state);
b8 load_texture(const char* texture_name, texture* t);

b8 texture_system_initialize(u64* memory_requirement, void* state, texture_system_config* config) {
    if(config->max_texture_count == 0){
        KFATAL("texture_system_initialize failed! max_texture_count must be greater than 0.");
        return false;
    }

    // block of memory will contain state structure, then block for array, then block for hashtable
    u64 struct_requirement = sizeof(texture_system_state);
    u64 array_requirement = sizeof(texture) * config->max_texture_count;
    u64 hashtable_requirement = sizeof(hashtable) + (sizeof(texture*) * config->max_texture_count);
    *memory_requirement = struct_requirement + array_requirement + hashtable_requirement;

    if (!state) {
        return true;
    }

    state_ptr = state;
    state_ptr->config = *config;

    // the array block is after the state. ALready allocated, so just point to it.
    void* array_block = state + struct_requirement;
    state_ptr->registered_textures = array_block;

    // Hashtable block is after array.
    void* hashtable_block = array_block + array_requirement;
    
    // create hashtable for texture lookup
    hashtable_create(sizeof(texture*), config->max_texture_count, hashtable_block, true, &state_ptr->registered_texture_table);

    // Fill the hashtable with invalid refernces to use as a default value for non-existent textures.
    texture_reference invalid_ref;
    invalid_ref.auto_release = false;
    invalid_ref.handle = INVALID_ID;
    invalid_ref.reference_count = 0;
    hashtable_fill(&state_ptr->registered_texture_table, &invalid_ref);

    

    //Invalidate all textures in the array
    u32 count = state_ptr->config.max_texture_count;
    for (u32 i = 0; i < count; ++i) {
        state_ptr->registered_textures[i].id = INVALID_ID;
        state_ptr->registered_textures[i].generation = INVALID_ID;

    }

    // create default textures for use in the system.
    create_default_textures(&state_ptr->default_texture);

    return true;
}

void create_texture(texture* texture) {
    kzero_memory(texture, sizeof(texture));
    texture->generation = INVALID_ID;
}

void texture_system_shutdown(void* state) {
    if(state_ptr){
        // Destroy all loaded textures
        for(u32 i = 0; i < state_ptr->config.max_texture_count; ++i){
            texture* t = &state_ptr->registered_textures[i];
            if(t->generation != INVALID_ID){
                renderer_destroy_texture(t);
            }
        }
        destroy_default_textures(state_ptr);

        state_ptr = 0;
    }
}

texture* texture_system_acquire(const char* name, b8 auto_release) {
    // Return default texture, but warn about it since this should be returned via get_deafult_texture 
    if(string_equali(name, DEFAULT_TEXTURE_NAME)) {
        KWARN("texture_system_acquire called for default texture. use get_default for default texture");
        return &state_ptr->default_texture;
    }

    texture_reference ref;
    if(state_ptr && hashtable_get(&state_ptr->registered_texture_table, name, &ref)) {
        // This ccan only changed the first time is loaded.
        if(ref.reference_count == 0) {
            ref.auto_release = auto_release;
        }
        ref.reference_count++;
        if (ref.handle == INVALID_ID) {
            // this means no textures exists here. Find a free index first 
            u32 count = state_ptr->config.max_texture_count;
            texture* t;
            for(u32 i = 0; i < count; ++i) {
                if(state_ptr->registered_textures[i].id == INVALID_ID) {
                    // a free slot has been found. use its index as the handle
                    ref.handle = i;
                    t = &state_ptr->registered_textures[i];
                    break;
                }
            }

            // Make sure an empty slot was actually found 
            if (!t || ref.handle == INVALID_ID) {
                KFATAL("texture_system_acquire - Texture system cannot hold any more textures, Adjust configuration to allow more");
                return 0;
            } 

            //create new texture 
            if(!load_texture(name, t)) {
                KERROR("Failed to load texture '%s'.", name);
                return 0;
            }

            // Also use the handle as the texture id.
            t->id = ref.handle;
        }
    }

    // NOTE: This would only happen in the event something went wrong with the state 
    KERROR("texture_system_acquire failed to acquire texture '%s'. NULL pointer will be returned. ", name);
    return 0;
} 

b8 create_default_textures(texture_system_state* state) {
    KTRACE("creating default texture...");
    const u32 tex_dimension = 256;
    const u32 channels = 4;
    const u32 pixel_count = tex_dimension * tex_dimension;
    // Allocate a contiguous pixel buffer (RGBA8)
    u8* pixels = (u8*)kallocate(pixel_count * channels * sizeof(u8), MEMORY_TAG_RENDERER);
    kset_memory(pixels, 255, pixel_count * channels * sizeof(u8));

    // each pixel
    for (u64 row = 0; row < tex_dimension; ++row) {
        for (u64 col = 0; col < tex_dimension; ++col) {
            u64 index = (row * tex_dimension) + col;
            u64 index_bpp = index * channels;
            if (row % 2) {
                if (col % 2) {
                    pixels[index_bpp + 0] = 0;
                    pixels[index_bpp + 1] = 0;
                }
            } else {
                if (!(col % 2)) {
                    pixels[index_bpp + 0] = 0;
                    pixels[index_bpp + 1] = 0;
                }
            }
        }
    }

    renderer_create_texture(
        DEFAULT_TEXTURE_NAME,
        tex_dimension,
        tex_dimension,
        channels,
        false,  // has_transparency
        pixels,
        &state->default_texture);

   // manually set the texture generation to invalid since this a default texture.
   state->default_texture.generation = INVALID_ID;


    return true;
}

void destroy_default_textures(texture_system_state* state) {
    if (state) {
        renderer_destroy_texture(&state->default_texture);
        state->default_texture.generation = INVALID_ID;
    }
}

b8 load_texture(const char* texture_name, texture* t) {
    char* format_string = "assets/textures/%s.%s";
    const i32 required_channel_count = 4;
    stbi_set_flip_vertically_on_load(true);
    i32 width, height, channel_count;
    char full_file_path[512];

    // TODO: Determine the file extension based on the file type. For now, we will assume all textures are PNGs. try different extensions
    string_format(full_file_path, format_string, texture_name, "png");

    // use a temp texture to load data
    texture temp_texture;

    u8* data = stbi_load(full_file_path, (i32*)&temp_texture.width, (i32*)&temp_texture.height, (i32*)&channel_count, required_channel_count);

    temp_texture.channel_count = required_channel_count;
    temp_texture.has_transparency = (channel_count == 4) ? true : false;

    if (data) {
        u32 current_generation = t->generation;
        t->generation = INVALID_ID;  // mark the texture as invalid until it is fully loaded
        u64 total_size = (u64)temp_texture.width * temp_texture.height * required_channel_count;
        // check for transperency in the texture data
        for (u64 i = 0; i < total_size; i += required_channel_count) {
            u8 alpha = data[i + 3];
            if (alpha < 255) {
                temp_texture.has_transparency = true;
                break;
            }
        }

        // acquire internal resources and upload to the GPU
        renderer_create_texture(
            texture_name,
            temp_texture.width,
            temp_texture.height,
            temp_texture.channel_count,
            temp_texture.has_transparency,
            data,
            &temp_texture);

        // take a copy of the old texture
        texture old = *t;
        *t = temp_texture;
        t->generation = current_generation + 1;  // increment the generation to indicate that

        // destroy the old texture if it was valid
        if (old.generation != INVALID_ID) {
            renderer_destroy_texture(&old);
        }

        stbi_image_free(data);
        return true;
    } else {
        if (stbi_failure_reason()) {
            KWARN("Failed to load texture: %s. Reason: %s", full_file_path, stbi_failure_reason());
        } else {
            KWARN("Failed to load texture: %s. Unknown reason.", full_file_path);
        }
        return false;
    }
}
