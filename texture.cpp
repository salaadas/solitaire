#include "texture.h"

i32 get_image_bytes_per_texel(Texture_Format tf)
{
    i32 channels_count = 0;

    switch (tf)
    {
        case Texture_Format::R8:        channels_count = 1; break;
        case Texture_Format::RG88:      channels_count = 2; break; 
        case Texture_Format::RGB888:    channels_count = 3; break; 

        case Texture_Format::ARGB8888:
        case Texture_Format::DEPTH32F:  channels_count = 4; break;

        default: {
            logprint("get_image_bytes_per_texel", "No texture format %d is not supported! Returning 0 as the channels count!\n", (i32)tf);
            assert(0);
            channels_count = 0;
        }
    }

    return channels_count;
}

void bitmap_alloc(Bitmap *bitmap, i32 w, i32 h, Texture_Format format)
{
    bitmap->width  = w;
    bitmap->height = h;

    auto bpp  = get_image_bytes_per_texel(format);
    auto size = w * h * bpp;

    bitmap->data                  = (u8*)(my_alloc(sizeof(u8) * size));
    // bitmap->length_in_bytes       = size;
    bitmap->XXX_num_mipmap_levels = 1;
    bitmap->XXX_format            = format;
}

Opengl_Texture_Info get_ogl_format(Texture_Format tf, bool srgb)
{
    Opengl_Texture_Info result;
    assert(tf != Texture_Format::UNINITIALIZE);

    switch (tf)
    {
        case Texture_Format::R8: {
            assert(srgb == false);
            result = { .dest_format=GL_R8, .src_format=GL_RED, .src_type=GL_UNSIGNED_BYTE };
        } break;
        case Texture_Format::RG88: { // @Investigate @Fixme Not tested!!!!
            assert(srgb == false);
            result = { .dest_format=GL_RG8, .src_format=GL_RG, .src_type=GL_UNSIGNED_BYTE };
        } break;
        case Texture_Format::RGB888: {
            GLenum dest_format = srgb ? GL_SRGB8 : GL_RGB8;
            result = {.dest_format=dest_format, .src_format=GL_RGB, .src_type=GL_UNSIGNED_BYTE, .alignment=1 };
        } break;
        case Texture_Format::ARGB8888: {
            GLenum dest_format = srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;
            result = { .dest_format=dest_format, .src_format=GL_RGBA, .src_type=GL_UNSIGNED_BYTE };
        } break;
        case Texture_Format::ARGBhalf: {
            assert(srgb == false);
            result = { .dest_format=GL_RGBA16F, .src_format=GL_RGBA, .src_type=GL_HALF_FLOAT };
        } break;
        case Texture_Format::DXT1: {
            GLenum dest_format = srgb ? GL_COMPRESSED_SRGB_S3TC_DXT1_EXT : GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
            result = { .dest_format=dest_format, .compressed=true, .block_size=8 };
        } break;
        case Texture_Format::DXT3: {
            GLenum dest_format = srgb ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT : GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
            result = { .dest_format=dest_format, .compressed=true, .block_size=16 };
        } break;
        case Texture_Format::DXT5: {
            GLenum dest_format = srgb ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT : GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
            result = { .dest_format=dest_format, .compressed=true, .block_size=16 };
        } break;
        case Texture_Format::DEPTH32F: {
            assert(srgb == false);
            result = { .dest_format=GL_DEPTH_COMPONENT32F, .src_format=GL_DEPTH_COMPONENT, .src_type=GL_FLOAT };
        } break;
        default: {
            logprint("get_ogl_format", "Texture format %d is unsupported!!!!\n", (i32)tf);
            assert(0);
        }; 
    }

    return result;
}

Texture_Format texture_format_with_this_nchannels(i32 channels_count)
{
    Texture_Format format;

    switch (channels_count)
    {
        case 1:  format = Texture_Format::R8;       break;
        case 2:  format = Texture_Format::RG88;     break;
        case 3:  format = Texture_Format::RGB888;   break;
        case 4:  format = Texture_Format::ARGB8888; break;
        default: {
            logprint("texture", "No texture format has %d color channels?!?!?!?\n", channels_count);
            assert(0);
            format = Texture_Format::UNINITIALIZE;
        };
    }

    return format;
}
