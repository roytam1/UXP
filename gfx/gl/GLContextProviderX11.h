/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLCONTEXTPROVIDER_X11_H_
#define GLCONTEXTPROVIDER_X11_H_

#include "GLContextTypes.h"
#include "SurfaceTypes.h"
#include "nsSize.h"

class nsIWidget;

namespace mozilla {
namespace widget {
class CompositorWidget;
}
namespace gl {

class GLContextProviderX11
{
public:
    static already_AddRefed<GLContext>
    CreateForCompositorWidget(widget::CompositorWidget* aCompositorWidget,
                              bool aForceAccelerated);

    static already_AddRefed<GLContext>
    CreateForWindow(nsIWidget* aWidget, bool aForceAccelerated);

    static already_AddRefed<GLContext>
    CreateOffscreen(const gfx::IntSize& aSize,
                    const SurfaceCaps& aMinCaps,
                    CreateContextFlags aFlags,
                    nsACString* const aOutFailureId);

    static already_AddRefed<GLContext>
    CreateHeadless(CreateContextFlags aFlags,
                   nsACString* const aOutFailureId);

    static already_AddRefed<GLContext>
    CreateWrappingExisting(void* aContext, void* aSurface);

    static GLContext*
    GetGlobalContext();

    static void
    Shutdown();
};

} // namespace gl
} // namespace mozilla

#endif /* GLCONTEXTPROVIDER_X11_H_ */
