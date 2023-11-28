/*
 * Copyright 2021 Rockchip Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * author: kevin.chen@rock-chips.com
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "Utils"
#include <utils/Log.h>

#include <sys/system_properties.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include "mpp_mem.h"
#include "Utils.h"
#include "RockchipRga.h"
#include "im2d.h"

using namespace android;

rga_buffer_handle_t importRgaBuffer(
        int32_t width, int32_t height, int32_t format, int32_t fd) {
    im_handle_param_t imParam;
    memset(&imParam, 0, sizeof(im_handle_param_t));

    imParam.width  = width;
    imParam.height = height;
    imParam.format = format;

    return importbuffer_fd(fd, &imParam);
}

void freeRgaBuffer(rga_buffer_handle_t handle) {
    releasebuffer_handle(handle);
}

void CommonUtil::dumpMppFrameToFile(MppFrame frame, FILE *file)
{
    int width   = 0;
    int height  = 0;
    int hStride = 0;
    int vStride = 0;
    MppFrameFormat fmt = MPP_FMT_YUV420SP;
    MppBuffer buffer   = NULL;
    uint8_t *base = NULL;

    if (NULL == file || NULL == frame)
        return;

    width   = mpp_frame_get_width(frame);
    height  = mpp_frame_get_height(frame);
    hStride = mpp_frame_get_hor_stride(frame);
    vStride = mpp_frame_get_ver_stride(frame);
    fmt     = mpp_frame_get_fmt(frame);
    buffer  = mpp_frame_get_buffer(frame);

    if (NULL == buffer)
        return;

    base = (uint8_t *)mpp_buffer_get_ptr(buffer);

    switch (fmt) {
    case MPP_FMT_YUV422SP : {
        /* YUV422SP -> YUV422P for better display */
        int i, j;
        uint8_t *base_y = base;
        uint8_t *base_c = base + hStride * vStride;
        uint8_t *tmp = mpp_malloc(uint8_t, hStride * height * 2);
        uint8_t *tmp_u = tmp;
        uint8_t *tmp_v = tmp + width * height / 2;

        for (i = 0; i < height; i++, base_y += hStride)
            fwrite(base_y, 1, width, file);

        for (i = 0; i < height; i++, base_c += hStride) {
            for (j = 0; j < width / 2; j++) {
                tmp_u[j] = base_c[2 * j + 0];
                tmp_v[j] = base_c[2 * j + 1];
            }
            tmp_u += width / 2;
            tmp_v += width / 2;
        }

        fwrite(tmp, 1, width * height, file);
        mpp_free(tmp);
    } break;
    case MPP_FMT_YUV420SP : {
        int i;
        uint8_t *base_y = base;
        uint8_t *base_c = base + hStride * vStride;

        for (i = 0; i < height; i++, base_y += hStride) {
            fwrite(base_y, 1, width, file);
        }
        for (i = 0; i < height / 2; i++, base_c += hStride) {
            fwrite(base_c, 1, width, file);
        }
    } break;
    case MPP_FMT_YUV420P : {
        int i;
        uint8_t *base_y = base;
        uint8_t *base_c = base + hStride * vStride;

        for (i = 0; i < height; i++, base_y += hStride) {
            fwrite(base_y, 1, width, file);
        }
        for (i = 0; i < height / 2; i++, base_c += hStride / 2) {
            fwrite(base_c, 1, width / 2, file);
        }
        for (i = 0; i < height / 2; i++, base_c += hStride / 2) {
            fwrite(base_c, 1, width / 2, file);
        }
    } break;
    case MPP_FMT_YUV444SP : {
        /* YUV444SP -> YUV444P for better display */
        int i, j;
        uint8_t *base_y = base;
        uint8_t *base_c = base + hStride * vStride;
        uint8_t *tmp = mpp_malloc(uint8_t, hStride * height * 2);
        uint8_t *tmp_u = tmp;
        uint8_t *tmp_v = tmp + width * height;

        for (i = 0; i < height; i++, base_y += hStride)
            fwrite(base_y, 1, width, file);

        for (i = 0; i < height; i++, base_c += hStride * 2) {
            for (j = 0; j < width; j++) {
                tmp_u[j] = base_c[2 * j + 0];
                tmp_v[j] = base_c[2 * j + 1];
            }
            tmp_u += width;
            tmp_v += width;
        }

        fwrite(tmp, 1, width * height * 2, file);
        mpp_free(tmp);
    } break;
    default : {
        ALOGE("not supported format %d", fmt);
    } break;
    }
}

void CommonUtil::dumpMppPacketToFile(MppPacket packet, FILE *file)
{
    uint8_t *data;
    int len;

    if (NULL == file || NULL == packet)
        return;

    data = (uint8_t*)mpp_packet_get_pos(packet);
    len = mpp_packet_get_length(packet);

    fwrite(data, 1, len, file);
    fflush(file);
}

void CommonUtil::dumpDataToFile(char *data, size_t size, FILE *file)
{
    if (NULL == file || NULL == data)
        return;

    fwrite(data, 1, size, file);
    fflush(file);
}

void CommonUtil::dumpDataToFile(char *data, size_t size, const char *fileName)
{
    FILE *file = NULL;

    file = fopen(fileName, "w+b");
    if (NULL == file) {
        ALOGE("failed to open file %s - %s", fileName, strerror(errno));
        return;
    }

    fwrite(data, 1, size, file);
    fflush(file);
    fclose(file);
}

void CommonUtil::dumpDmaFdToFile(int fd, size_t size, FILE *file)
{
    void *ptr = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (ptr != NULL) {
        fwrite(ptr, 1, size, file);
        fflush(file);
    } else {
        ALOGE("failed to map fd value %d", fd);
    }
}

MPP_RET CommonUtil::storeFileData(const char *file_name, char **data, size_t *length)
{
    FILE   *file    = NULL;
    size_t  fileLen = 0;

    file = fopen(file_name, "rb");
    if (NULL == file) {
        ALOGE("failed to open file %s - %s", file_name, strerror(errno));
        return MPP_NOK;
    }

    fseek(file, 0L, SEEK_END);
    fileLen = ftell(file);
    rewind(file);

    *data = (char *)malloc(fileLen);
    if (NULL == *data) {
        ALOGE("failed to malloc buffer - file %s", file_name);
        fclose(file);
        return MPP_NOK;
    }

    fread(*data, 1, fileLen, file);
    *length = fileLen;

    fclose(file);

    return MPP_OK;
}

MPP_RET CommonUtil::cropImage(
        int src, int dst, int srcWidth, int srcHeight,
        int srcWstride, int srcHstride, int dstWidth, int dstHeight)
{
    MPP_RET ret = MPP_OK;
    int srcFormat, dstFormat;
    rga_info_t rgasrc, rgadst;
    rga_buffer_handle_t srcHdl;
    rga_buffer_handle_t dstHdl;

    RockchipRga& rkRga(RockchipRga::get());

    srcFormat = dstFormat = HAL_PIXEL_FORMAT_YCrCb_NV12;

    memset(&rgasrc, 0, sizeof(rga_info_t));
    memset(&rgadst, 0, sizeof(rga_info_t));

    srcHdl = importRgaBuffer(srcWidth, srcHeight, srcFormat, src);
    dstHdl = importRgaBuffer(dstWidth, dstHeight, dstFormat, dst);
    if (!srcHdl || !dstHdl) {
        ALOGE("failed to import rga buffer");
        return MPP_NOK;
    }

    rgasrc.handle = srcHdl;
    rgadst.handle = dstHdl;
    rga_set_rect(&rgasrc.rect, 0, 0, srcWidth, srcHeight,
                 srcWstride, srcHstride, srcFormat);
    rga_set_rect(&rgadst.rect, 0, 0, dstWidth, dstHeight,
                 dstWidth, dstHeight, dstFormat);

    if (rkRga.RkRgaBlit(&rgasrc, &rgadst, NULL)) {
        ALOGE("failed to rga blit");
        ret = MPP_NOK;
    }

    freeRgaBuffer(srcHdl);
    freeRgaBuffer(dstHdl);

    return ret;
}

MPP_RET CommonUtil::readImage(
        char *src, char *dst, int width, int height,
        int wstride, int hstride, MppFrameFormat fmt)
{
    MPP_RET ret = MPP_OK;
    int row = 0;
    char *bufY = dst;
    char *bufU = bufY + wstride * hstride;
    char *bufV = bufU + wstride * hstride / 4;

    switch (fmt) {
    case MPP_FMT_YUV420SP : {
        for (row = 0; row < height; row++) {
            memcpy(bufY + row * wstride, src, width);
            src += width;
        }

        for (row = 0; row < height / 2; row++) {
            memcpy(bufU + row * wstride, src, width);
            src += width;
        }
    } break;
    case MPP_FMT_YUV420P : {
        for (row = 0; row < height; row++) {
            memcpy(bufY + row * wstride, src, width);
            src += width;
        }

        for (row = 0; row < height / 2; row++) {
            memcpy(bufU + row * wstride / 2, src, width / 2);
            src += width / 2;
        }

        for (row = 0; row < height / 2; row++) {
            memcpy(bufV + row * wstride / 2, src, width / 2);
            src += width / 2;
        }
    } break;
    case MPP_FMT_RGBA8888 :
    case MPP_FMT_ABGR8888 :
    case MPP_FMT_ARGB8888 : {
        for (row = 0; row < height; row++) {
            memcpy(bufY + row * wstride * 4, src, width * 4);
            src += width * 4;
        }
    } break;
    case MPP_FMT_YUV422_YUYV :
    case MPP_FMT_YUV422_UYVY : {
        for (row = 0; row < height; row++) {
            memcpy(bufY + row * wstride * 2, src, width * 2);
            src += width * 2;
        }
    } break;
    case MPP_FMT_RGB888 :
    case MPP_FMT_BGR888 : {
    for (row = 0; row < height; row++) {
        memcpy(bufY + row * wstride * 3, src, width * 3);
        src += width * 3;
    }
    } break;
    default : {
        ALOGE("read image do not support fmt %d", fmt);
        ret = MPP_ERR_VALUE;
    } break;
    }

    return ret;
}

int CommonUtil::envGetU32(const char *name, uint32_t *value, uint32_t defaultValue)
{
   char prop[PROP_VALUE_MAX + 1];
   int len = __system_property_get(name, prop);
   if (len > 0) {
       char *endptr;
       int base = (prop[0] == '0' && prop[1] == 'x') ? (16) : (10);
       errno = 0;
       *value = strtoul(prop, &endptr, base);
       if (errno || (prop == endptr)) {
           errno = 0;
           *value = defaultValue;
       }
   } else {
       *value = defaultValue;
   }

   return 0;
}

int CommonUtil::envGetStr(const char *name, const char **value, const char *defaultValue)
{
   static unsigned char envStr[2][PROP_VALUE_MAX + 1];
   static int32_t envIdx = 0;
   char *prop = reinterpret_cast<char *>(envStr[envIdx]);
   int len = __system_property_get(name, prop);
   if (len > 0) {
       *value  = prop;
       envIdx = !envIdx;
   } else {
       *value = defaultValue;
   }

   return 0;
}

int CommonUtil::envSetU32(const char *name, uint32_t value)
{
   char buf[PROP_VALUE_MAX + 1 + 2];
   snprintf(buf, sizeof(buf), "0x%x", value);
   int len = __system_property_set(name, buf);
   return (len) ? (0) : (-1);
}

int CommonUtil::envSetStr(const char *name, char *value)
{
   int len = __system_property_set(name, value);
   return (len) ? (0) : (-1);
}

bool CommonUtil::isValidDmaFd(int fd)
{
    /* detect input file handle */
    int fsFlag = fcntl(fd, F_GETFL, NULL);
    int fdFlag = fcntl(fd, F_GETFD, NULL);
    if (fsFlag == -1 || fdFlag == -1) {
        return false;;
    }

    return true;
}

void CommonUtil::setPerformanceMode(int on)
{
    int fd = -1;

    fd = open("/sys/class/devfreq/dmc/system_status", O_WRONLY);
    if (fd  == -1) {
        ALOGD("failed to open /sys/class/devfreq/dmc/system_status");
    }

    if (fd != -1) {
        ALOGD("%s performance mode", (on == 1) ? "config" : "clear");
        write(fd, (on == 1) ? "p" : "n", 1);
        close(fd);
    } else {
        ALOGD("failed to open /sys/class/devfreq/dmc/system_status");
    }
}
