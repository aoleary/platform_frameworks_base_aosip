/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you mPrimitiveFields.may not use this file except in compliance with the License.
 * You mPrimitiveFields.may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "OpenGLRenderer"

#include "RenderProperties.h"

#include <utils/Trace.h>

#include <SkCanvas.h>
#include <SkMatrix.h>
#include <SkPath.h>
#include <SkPathOps.h>

#include "Matrix.h"
#include "utils/MathUtils.h"

namespace android {
namespace uirenderer {

RenderProperties::PrimitiveFields::PrimitiveFields()
        : mClipToBounds(true)
        , mProjectBackwards(false)
        , mProjectionReceiver(false)
        , mAlpha(1)
        , mHasOverlappingRendering(true)
        , mTranslationX(0), mTranslationY(0), mTranslationZ(0)
        , mRotation(0), mRotationX(0), mRotationY(0)
        , mScaleX(1), mScaleY(1)
        , mPivotX(0), mPivotY(0)
        , mLeft(0), mTop(0), mRight(0), mBottom(0)
        , mWidth(0), mHeight(0)
        , mPivotExplicitlySet(false)
        , mMatrixOrPivotDirty(false)
        , mCaching(false) {
}

RenderProperties::ComputedFields::ComputedFields()
        : mTransformMatrix(NULL)
        , mClipPath(NULL) {
}

RenderProperties::ComputedFields::~ComputedFields() {
    delete mTransformMatrix;
    delete mClipPath;
}

RenderProperties::RenderProperties()
        : mStaticMatrix(NULL)
        , mAnimationMatrix(NULL) {
}

RenderProperties::~RenderProperties() {
    delete mStaticMatrix;
    delete mAnimationMatrix;
}

RenderProperties& RenderProperties::operator=(const RenderProperties& other) {
    if (this != &other) {
        mPrimitiveFields = other.mPrimitiveFields;
        setStaticMatrix(other.getStaticMatrix());
        setAnimationMatrix(other.getAnimationMatrix());
        setCameraDistance(other.getCameraDistance());

        // Update the computed clip path
        updateClipPath();

        // Force recalculation of the matrix, since other's dirty bit may be clear
        mPrimitiveFields.mMatrixOrPivotDirty = true;
        updateMatrix();
    }
    return *this;
}

void RenderProperties::debugOutputProperties(const int level) const {
    if (mPrimitiveFields.mLeft != 0 || mPrimitiveFields.mTop != 0) {
        ALOGD("%*sTranslate (left, top) %d, %d", level * 2, "", mPrimitiveFields.mLeft, mPrimitiveFields.mTop);
    }
    if (mStaticMatrix) {
        ALOGD("%*sConcatMatrix (static) %p: " SK_MATRIX_STRING,
                level * 2, "", mStaticMatrix, SK_MATRIX_ARGS(mStaticMatrix));
    }
    if (mAnimationMatrix) {
        ALOGD("%*sConcatMatrix (animation) %p: " SK_MATRIX_STRING,
                level * 2, "", mAnimationMatrix, SK_MATRIX_ARGS(mAnimationMatrix));
    }
    if (hasTransformMatrix()) {
        if (isTransformTranslateOnly()) {
            ALOGD("%*sTranslate %.2f, %.2f, %.2f",
                    level * 2, "", mPrimitiveFields.mTranslationX, mPrimitiveFields.mTranslationY, mPrimitiveFields.mTranslationZ);
        } else {
            ALOGD("%*sConcatMatrix %p: " SK_MATRIX_STRING,
                    level * 2, "", mComputedFields.mTransformMatrix, SK_MATRIX_ARGS(mComputedFields.mTransformMatrix));
        }
    }

    bool clipToBoundsNeeded = mPrimitiveFields.mCaching ? false : mPrimitiveFields.mClipToBounds;
    if (mPrimitiveFields.mAlpha < 1) {
        if (mPrimitiveFields.mCaching) {
            ALOGD("%*sSetOverrideLayerAlpha %.2f", level * 2, "", mPrimitiveFields.mAlpha);
        } else if (!mPrimitiveFields.mHasOverlappingRendering) {
            ALOGD("%*sScaleAlpha %.2f", level * 2, "", mPrimitiveFields.mAlpha);
        } else {
            int flags = SkCanvas::kHasAlphaLayer_SaveFlag;
            if (clipToBoundsNeeded) {
                flags |= SkCanvas::kClipToLayer_SaveFlag;
                clipToBoundsNeeded = false; // clipping done by save layer
            }
            ALOGD("%*sSaveLayerAlpha %d, %d, %d, %d, %d, 0x%x", level * 2, "",
                    0, 0, getWidth(), getHeight(),
                    (int)(mPrimitiveFields.mAlpha * 255), flags);
        }
    }
    if (clipToBoundsNeeded) {
        ALOGD("%*sClipRect %d, %d, %d, %d", level * 2, "",
                0, 0, getWidth(), getHeight());
    }
}

void RenderProperties::updateMatrix() {
    if (mPrimitiveFields.mMatrixOrPivotDirty) {
        if (!mComputedFields.mTransformMatrix) {
            // only allocate a mPrimitiveFields.matrix if we have a complex transform
            mComputedFields.mTransformMatrix = new SkMatrix();
        }
        if (!mPrimitiveFields.mPivotExplicitlySet) {
            mPrimitiveFields.mPivotX = mPrimitiveFields.mWidth / 2.0f;
            mPrimitiveFields.mPivotY = mPrimitiveFields.mHeight / 2.0f;
        }
        SkMatrix* transform = mComputedFields.mTransformMatrix;
        transform->reset();
        if (MathUtils::isZero(getRotationX()) && MathUtils::isZero(getRotationY())) {
            transform->setTranslate(getTranslationX(), getTranslationY());
            transform->preRotate(getRotation(), getPivotX(), getPivotY());
            transform->preScale(getScaleX(), getScaleY(), getPivotX(), getPivotY());
        } else {
            SkMatrix transform3D;
            mComputedFields.mTransformCamera.save();
            transform->preScale(getScaleX(), getScaleY(), getPivotX(), getPivotY());
            mComputedFields.mTransformCamera.rotateX(mPrimitiveFields.mRotationX);
            mComputedFields.mTransformCamera.rotateY(mPrimitiveFields.mRotationY);
            mComputedFields.mTransformCamera.rotateZ(-mPrimitiveFields.mRotation);
            mComputedFields.mTransformCamera.getMatrix(&transform3D);
            transform3D.preTranslate(-getPivotX(), -getPivotY());
            transform3D.postTranslate(getPivotX() + getTranslationX(),
                    getPivotY() + getTranslationY());
            transform->postConcat(transform3D);
            mComputedFields.mTransformCamera.restore();
        }
        mPrimitiveFields.mMatrixOrPivotDirty = false;
    }
}

void RenderProperties::updateClipPath() {
    const SkPath* outlineClipPath = mPrimitiveFields.mOutline.willClip()
            ? mPrimitiveFields.mOutline.getPath() : NULL;
    const SkPath* revealClipPath = mPrimitiveFields.mRevealClip.getPath();

    if (!outlineClipPath && !revealClipPath) {
        // mComputedFields.mClipPath doesn't need to be updated, since it won't be used
        return;
    }

    if (mComputedFields.mClipPath == NULL) {
        mComputedFields.mClipPath = new SkPath();
    }
    SkPath* clipPath = mComputedFields.mClipPath;
    mComputedFields.mClipPathOp = SkRegion::kIntersect_Op;

    if (outlineClipPath && revealClipPath) {
        SkPathOp op = kIntersect_PathOp;
        if (mPrimitiveFields.mRevealClip.isInverseClip()) {
            op = kDifference_PathOp; // apply difference step in the Op below, instead of draw time
        }

        Op(*outlineClipPath, *revealClipPath, op, clipPath);
    } else if (outlineClipPath) {
        *clipPath = *outlineClipPath;
    } else {
        *clipPath = *revealClipPath;
        if (mPrimitiveFields.mRevealClip.isInverseClip()) {
            // apply difference step at draw time
            mComputedFields.mClipPathOp = SkRegion::kDifference_Op;
        }
    }
}

} /* namespace uirenderer */
} /* namespace android */
