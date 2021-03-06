/*
 * Implementation of MedaType utility functions
 *
 * Copyright 2003 Robert Shearman
 * Copyright 2010 Aric Stewart, CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "strmbase_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(strmbase);

static const struct
{
    const GUID *guid;
    const char *name;
}
strmbase_guids[] =
{
#define X(g) {&(g), #g}
    X(GUID_NULL),

#undef OUR_GUID_ENTRY
#define OUR_GUID_ENTRY(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) X(name),
#include "uuids.h"

#undef X
};

static const char *strmbase_debugstr_guid(const GUID *guid)
{
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(strmbase_guids); ++i)
    {
        if (IsEqualGUID(strmbase_guids[i].guid, guid))
            return wine_dbg_sprintf("%s", strmbase_guids[i].name);
    }

    return debugstr_guid(guid);
}

static const char *debugstr_fourcc(DWORD fourcc)
{
    char str[4] = {fourcc, fourcc >> 8, fourcc >> 16, fourcc >> 24};
    if (isprint(str[0]) && isprint(str[1]) && isprint(str[2]) && isprint(str[3]))
        return wine_dbgstr_an(str, 4);
    return wine_dbg_sprintf("%#x", fourcc);
}

void strmbase_dump_media_type(const AM_MEDIA_TYPE *mt)
{
    if (!TRACE_ON(strmbase) || !mt) return;

    TRACE("Dumping media type %p: major type %s, subtype %s",
            mt, strmbase_debugstr_guid(&mt->majortype), strmbase_debugstr_guid(&mt->subtype));
    if (mt->bFixedSizeSamples) TRACE(", fixed size samples");
    if (mt->bTemporalCompression) TRACE(", temporal compression");
    if (mt->lSampleSize) TRACE(", sample size %d", mt->lSampleSize);
    if (mt->pUnk) TRACE(", pUnk %p", mt->pUnk);
    TRACE(", format type %s.\n", strmbase_debugstr_guid(&mt->formattype));

    if (!mt->pbFormat) return;

    TRACE("Dumping format %p: ", mt->pbFormat);

    if (IsEqualGUID(&mt->formattype, &FORMAT_WaveFormatEx) && mt->cbFormat >= sizeof(WAVEFORMATEX))
    {
        WAVEFORMATEX *wfx = (WAVEFORMATEX *)mt->pbFormat;

        TRACE("tag %#x, %u channels, sample rate %u, %u bytes/sec, alignment %u, %u bits/sample.\n",
                wfx->wFormatTag, wfx->nChannels, wfx->nSamplesPerSec,
                wfx->nAvgBytesPerSec, wfx->nBlockAlign, wfx->wBitsPerSample);

        if (wfx->cbSize)
        {
            const unsigned char *extra = (const unsigned char *)(wfx + 1);
            unsigned int i;

            TRACE("  Extra bytes:");
            for (i = 0; i < wfx->cbSize; ++i)
            {
                if (!(i % 16)) TRACE("\n     ");
                TRACE(" %02x", extra[i]);
            }
            TRACE("\n");
        }
    }
    else if (IsEqualGUID(&mt->formattype, &FORMAT_VideoInfo) && mt->cbFormat >= sizeof(VIDEOINFOHEADER))
    {
        VIDEOINFOHEADER *vih = (VIDEOINFOHEADER *)mt->pbFormat;

        TRACE("source %s, target %s, bitrate %u, error rate %u, %s sec/frame, ",
                wine_dbgstr_rect(&vih->rcSource), wine_dbgstr_rect(&vih->rcTarget),
                vih->dwBitRate, vih->dwBitErrorRate, debugstr_time(vih->AvgTimePerFrame));
        TRACE("size %dx%d, %u planes, %u bpp, compression %s, image size %u",
                vih->bmiHeader.biWidth, vih->bmiHeader.biHeight, vih->bmiHeader.biPlanes,
                vih->bmiHeader.biBitCount, debugstr_fourcc(vih->bmiHeader.biCompression),
                vih->bmiHeader.biSizeImage);
        if (vih->bmiHeader.biXPelsPerMeter || vih->bmiHeader.biYPelsPerMeter)
            TRACE(", resolution %dx%d", vih->bmiHeader.biXPelsPerMeter, vih->bmiHeader.biYPelsPerMeter);
        if (vih->bmiHeader.biClrUsed) TRACE(", %d colours", vih->bmiHeader.biClrUsed);
        if (vih->bmiHeader.biClrImportant) TRACE(", %d important colours", vih->bmiHeader.biClrImportant);
        TRACE(".\n");
    }
    else
        TRACE("not implemented for this format type.\n");
}

HRESULT WINAPI CopyMediaType(AM_MEDIA_TYPE *dest, const AM_MEDIA_TYPE *src)
{
    *dest = *src;
    if (src->pbFormat)
    {
        dest->pbFormat = CoTaskMemAlloc(src->cbFormat);
        if (!dest->pbFormat)
            return E_OUTOFMEMORY;
        memcpy(dest->pbFormat, src->pbFormat, src->cbFormat);
    }
    if (dest->pUnk)
        IUnknown_AddRef(dest->pUnk);
    return S_OK;
}

void WINAPI FreeMediaType(AM_MEDIA_TYPE * pMediaType)
{
    CoTaskMemFree(pMediaType->pbFormat);
    pMediaType->pbFormat = NULL;
    if (pMediaType->pUnk)
    {
        IUnknown_Release(pMediaType->pUnk);
        pMediaType->pUnk = NULL;
    }
}

AM_MEDIA_TYPE * WINAPI CreateMediaType(AM_MEDIA_TYPE const * pSrc)
{
    AM_MEDIA_TYPE * pDest;

    pDest = CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE));
    if (!pDest)
        return NULL;

    if (FAILED(CopyMediaType(pDest, pSrc)))
    {
        CoTaskMemFree(pDest);
        return NULL;
    }

    return pDest;
}

void WINAPI DeleteMediaType(AM_MEDIA_TYPE * pMediaType)
{
    FreeMediaType(pMediaType);
    CoTaskMemFree(pMediaType);
}
