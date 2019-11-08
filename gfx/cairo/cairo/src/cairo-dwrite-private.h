/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <dwrite_1.h>
#include <d2d1.h>

// DirectWrite is not available on all platforms.
typedef HRESULT (WINAPI*DWriteCreateFactoryFunc)(
  DWRITE_FACTORY_TYPE factoryType,
  REFIID iid,
  IUnknown **factory
);

/* cairo_scaled_font_t implementation */
struct _cairo_dwrite_scaled_font {
    cairo_scaled_font_t base;
    cairo_matrix_t mat;
    cairo_matrix_t mat_inverse;
    cairo_antialias_t antialias_mode;
    DWRITE_MEASURING_MODE measuring_mode;
    cairo_bool_t manual_show_glyphs_allowed;
    cairo_d2d_surface_t::TextRenderingState rendering_mode;
};
typedef struct _cairo_dwrite_scaled_font cairo_dwrite_scaled_font_t;

class DWriteFactory
{
public:
    static IDWriteFactory *Instance()
    {
	if (!mFactoryInstance) {
	    DWriteCreateFactoryFunc createDWriteFactory = (DWriteCreateFactoryFunc)
		GetProcAddress(LoadLibraryW(L"dwrite.dll"), "DWriteCreateFactory");
	    if (createDWriteFactory) {
		HRESULT hr = createDWriteFactory(
		    DWRITE_FACTORY_TYPE_SHARED,
		    __uuidof(IDWriteFactory),
		    reinterpret_cast<IUnknown**>(&mFactoryInstance));
		assert(SUCCEEDED(hr));
	    }
	}
	return mFactoryInstance;
    }

    static IDWriteFontCollection *SystemCollection()
    {
	if (!mSystemCollection) {
	    if (Instance()) {
		HRESULT hr = Instance()->GetSystemFontCollection(&mSystemCollection);
		assert(SUCCEEDED(hr));
	    }
	}
	return mSystemCollection;
    }

    static IDWriteFontFamily *FindSystemFontFamily(const WCHAR *aFamilyName)
    {
	UINT32 idx;
	BOOL found;
	if (!SystemCollection()) {
	    return NULL;
	}
	SystemCollection()->FindFamilyName(aFamilyName, &idx, &found);
	if (!found) {
	    return NULL;
	}

	IDWriteFontFamily *family;
	SystemCollection()->GetFontFamily(idx, &family);
	return family;
    }

    static IDWriteRenderingParams *RenderingParams(cairo_d2d_surface_t::TextRenderingState mode)
    {
	if (!mDefaultRenderingParams ||
            !mForceGDIClassicRenderingParams ||
            !mCustomClearTypeRenderingParams)
        {
	    CreateRenderingParams();
	}
	IDWriteRenderingParams *params;
        if (mode == cairo_d2d_surface_t::TEXT_RENDERING_NO_CLEARTYPE) {
            params = mDefaultRenderingParams;
        } else if (mode == cairo_d2d_surface_t::TEXT_RENDERING_GDI_CLASSIC && mRenderingMode < 0) {
            params = mForceGDIClassicRenderingParams;
        } else {
            params = mCustomClearTypeRenderingParams;
        }
	if (params) {
	    params->AddRef();
	}
	return params;
    }

    static void SetRenderingParams(FLOAT aGamma,
				   FLOAT aEnhancedContrast,
				   FLOAT aClearTypeLevel,
				   int aPixelGeometry,
				   int aRenderingMode)
    {
	mGamma = aGamma;
	mEnhancedContrast = aEnhancedContrast;
	mClearTypeLevel = aClearTypeLevel;
        mPixelGeometry = aPixelGeometry;
        mRenderingMode = aRenderingMode;
	// discard any current RenderingParams objects
	if (mCustomClearTypeRenderingParams) {
	    mCustomClearTypeRenderingParams->Release();
	    mCustomClearTypeRenderingParams = NULL;
	}
	if (mForceGDIClassicRenderingParams) {
	    mForceGDIClassicRenderingParams->Release();
	    mForceGDIClassicRenderingParams = NULL;
	}
	if (mDefaultRenderingParams) {
	    mDefaultRenderingParams->Release();
	    mDefaultRenderingParams = NULL;
	}
    }

    static int GetClearTypeRenderingMode() {
        return mRenderingMode;
    }

private:
    static void CreateRenderingParams();

    static IDWriteFactory *mFactoryInstance;
    static IDWriteFontCollection *mSystemCollection;
    static IDWriteRenderingParams *mDefaultRenderingParams;
    static IDWriteRenderingParams *mCustomClearTypeRenderingParams;
    static IDWriteRenderingParams *mForceGDIClassicRenderingParams;
    static FLOAT mGamma;
    static FLOAT mEnhancedContrast;
    static FLOAT mClearTypeLevel;
    static int mPixelGeometry;
    static int mRenderingMode;
};

class AutoDWriteGlyphRun : public DWRITE_GLYPH_RUN
{
    static const int kNumAutoGlyphs = 256;

public:
    AutoDWriteGlyphRun() {
        glyphCount = 0;
    }

    ~AutoDWriteGlyphRun() {
        if (glyphCount > kNumAutoGlyphs) {
            delete[] glyphIndices;
            delete[] glyphAdvances;
            delete[] glyphOffsets;
        }
    }

    void allocate(int aNumGlyphs) {
        glyphCount = aNumGlyphs;
        if (aNumGlyphs <= kNumAutoGlyphs) {
            glyphIndices = &mAutoIndices[0];
            glyphAdvances = &mAutoAdvances[0];
            glyphOffsets = &mAutoOffsets[0];
        } else {
            glyphIndices = new UINT16[aNumGlyphs];
            glyphAdvances = new FLOAT[aNumGlyphs];
            glyphOffsets = new DWRITE_GLYPH_OFFSET[aNumGlyphs];
        }
    }

private:
    DWRITE_GLYPH_OFFSET mAutoOffsets[kNumAutoGlyphs];
    FLOAT               mAutoAdvances[kNumAutoGlyphs];
    UINT16              mAutoIndices[kNumAutoGlyphs];
};

/* cairo_font_face_t implementation */
struct _cairo_dwrite_font_face {
    cairo_font_face_t base;
    IDWriteFont *font;
    IDWriteFontFace *dwriteface;
};
typedef struct _cairo_dwrite_font_face cairo_dwrite_font_face_t;

DWRITE_MATRIX _cairo_dwrite_matrix_from_matrix(const cairo_matrix_t *matrix);

// This will initialize a DWrite glyph run from cairo glyphs and a scaled_font.
void
_cairo_dwrite_glyph_run_from_glyphs(cairo_glyph_t *glyphs,
				    int num_glyphs,
				    cairo_dwrite_scaled_font_t *scaled_font,
				    AutoDWriteGlyphRun *run,
				    cairo_bool_t *transformed);
