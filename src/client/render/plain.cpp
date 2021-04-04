/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2017 numzero, Lobachevskiy Vitaliy <numzer0@yandex.ru>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "plain.h"
#include "settings.h"

#include "client/client.h"
#include "client/shader.h"
#include "client/tile.h"

// With the IShaderConstantSetter, uniforms of the shader can be set (probably)
class OversampleShaderConstantSetter : public IShaderConstantSetter
{
public:
	OversampleShaderConstantSetter(RenderingCorePlain *core) :
			m_core(core), m_resolution("resolution")
	{
	}

	~OversampleShaderConstantSetter() override = default;

	void onSetConstants(video::IMaterialRendererServices *services) override
	{
		// TODO: Why is the resolution uniform never set?

		v2u32 render_size = m_core->getScreensize();
		float as_array[2] = {
				(float)render_size.X,
				(float)render_size.Y,
		};
		m_resolution.set(as_array, services);
	}

	//~ void onSetMaterial(const video::SMaterial& material) override
	//~ {
	//~ m_render_size = render_size;
	//~ }

private:
	RenderingCorePlain *m_core;
	CachedPixelShaderSetting<float, 2> m_resolution;
};

// Each shader requires a constant setter and a factory for it
class OversampleShaderConstantSetterFactory : public IShaderConstantSetterFactory
{
	RenderingCorePlain *m_core;

public:
	OversampleShaderConstantSetterFactory(RenderingCorePlain *core) : m_core(core) {}

	virtual IShaderConstantSetter *create()
	{
		return new OversampleShaderConstantSetter(m_core);
	}
};

inline u32 scaledown(u32 coef, u32 size)
{
	return (size + coef - 1) / coef;
}

RenderingCorePlain::RenderingCorePlain(
	IrrlichtDevice *_device, Client *_client, Hud *_hud)
	: RenderingCore(_device, _client, _hud)
{
	scale = g_settings->getU16("undersampling");

	IWritableShaderSource *s = client->getShaderSource();
	s->addShaderConstantSetterFactory(
			new OversampleShaderConstantSetterFactory(this));
	u32 shader = s->getShader("fxaa", TILE_MATERIAL_BASIC, NDT_NORMAL);
	renderMaterial.UseMipMaps = false;
	renderMaterial.MaterialType = s->getShaderInfo(shader).material;
	renderMaterial.TextureLayer[0].AnisotropicFilter = false;
	renderMaterial.TextureLayer[0].BilinearFilter = false;
	renderMaterial.TextureLayer[0].TrilinearFilter = false;
	renderMaterial.TextureLayer[0].TextureWrapU = video::ETC_CLAMP_TO_EDGE;
	renderMaterial.TextureLayer[0].TextureWrapV = video::ETC_CLAMP_TO_EDGE;

}

void RenderingCorePlain::initTextures()
{
	rendered = driver->addRenderTargetTexture(screensize, "3d_render",
			//~ video::ECF_A8R8G8B8);
			video::ECF_A16B16G16R16F);
	renderMaterial.TextureLayer[0].Texture = rendered;

	if (scale <= 1)
		return;
	v2u32 size{scaledown(scale, screensize.X), scaledown(scale, screensize.Y)};
	lowres = driver->addRenderTargetTexture(
			size, "render_lowres", video::ECF_A8R8G8B8);
}

void RenderingCorePlain::clearTextures()
{
	driver->removeTexture(rendered);

	if (scale <= 1)
		return;
	driver->removeTexture(lowres);
}

void RenderingCorePlain::beforeDraw()
{
	if (scale <= 1)
		return;
	driver->setRenderTarget(lowres, true, true, skycolor);
}

void RenderingCorePlain::upscale()
{
	if (scale <= 1)
		return;
	driver->setRenderTarget(0, true, true);
	v2u32 size{scaledown(scale, screensize.X), scaledown(scale, screensize.Y)};
	v2u32 dest_size{scale * size.X, scale * size.Y};
	driver->draw2DImage(lowres, core::rect<s32>(0, 0, dest_size.X, dest_size.Y),
			core::rect<s32>(0, 0, size.X, size.Y));
}

void RenderingCorePlain::drawAll()
{
	driver->setRenderTarget(rendered, true, true, skycolor);
	draw3D();
	driver->setRenderTarget(nullptr, false, false, skycolor);
	static const video::S3DVertex vertices[4] = {
			video::S3DVertex(1.0, -1.0, 0.0, 0.0, 0.0, -1.0,
					video::SColor(255, 0, 255, 255), 1.0, 0.0),
			video::S3DVertex(-1.0, -1.0, 0.0, 0.0, 0.0, -1.0,
					video::SColor(255, 255, 0, 255), 0.0, 0.0),
			video::S3DVertex(-1.0, 1.0, 0.0, 0.0, 0.0, -1.0,
					video::SColor(255, 255, 255, 0), 0.0, 1.0),
			video::S3DVertex(1.0, 1.0, 0.0, 0.0, 0.0, -1.0,
					video::SColor(255, 255, 255, 255), 1.0, 1.0),
	};
	static const u16 indices[6] = {0, 1, 2, 2, 3, 0};
	driver->setMaterial(renderMaterial);
	driver->drawVertexPrimitiveList(&vertices, 4, &indices, 2);

	drawPostFx();
	// upscale();
	drawHUD();
}
