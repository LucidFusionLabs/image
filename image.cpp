/*
 * $Id$
 * Copyright (C) 2009 Lucid Fusion Labs

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "core/app/app.h"
#include "core/app/shell.h"
#include "core/app/gl/view.h"

namespace LFL {
DEFINE_string(output, "", "Output");
DEFINE_string(input, "", "Input");
DEFINE_bool(input_3D, false, "3d input");
DEFINE_float(input_scale, 0, "Input scale");
DEFINE_string(input_prims, "", "Comma separated list of .obj primitives to highlight");
DEFINE_string(input_filter, "", "Filter type [dark2alpha]");
DEFINE_bool(visualize, false, "Display");
DEFINE_string(shader, "", "Apply this shader");
DEFINE_string(make_png_atlas, "", "Build PNG atlas with files in directory");
DEFINE_int(make_png_atlas_width, 256, "Build PNG atlas with this width");
DEFINE_int(make_png_atlas_height, 256, "Build PNG atlas with this height");
DEFINE_string(split_png_atlas, "", "Split PNG atlas back into individual files");
DEFINE_string(filter_png_atlas, "", "Filter PNG atlas");
DEFINE_string(paste_image, "", "Paste image filename");
DEFINE_string(paste_to, "", "[rect]:[x,y,w,h]");

struct MyApp : public Application {
  Shader shader;
  MyApp(int ac, const char* const* av) : Application(ac, av), shader(this) {}
  void OnWindowInit(Window *W);
  void OnWindowStart(Window *W);
} *app;

struct MyView : public View {
  Scene scene;
  MyView(Window *W) : View(W) {}

  void DrawInput3D(GraphicsDevice *gd, Asset *a, Entity *e) {
    if (!FLAGS_input_prims.empty() && a && a->geometry) {
      vector<int> highlight_input_prims;
      Split(FLAGS_input_prims, isint2<',', ' '>, &highlight_input_prims);

      gd->DisableLighting();
      gd->Color4f(1.0, 0.0, 0.0, 1.0);
      int vpp = GraphicsDevice::VertsPerPrimitive(a->geometry->primtype);
      for (auto i = highlight_input_prims.begin(); i != highlight_input_prims.end(); ++i) {
        CHECK_LT(*i, a->geometry->count / vpp);
        scene.Draw(gd, a->geometry, e, *i * vpp, vpp);
      }
      gd->Color4f(1.0, 1.0, 1.0, 1.0);
    }
  }

  void Frame3D(LFL::Window *W, unsigned clicks, int flag) {
    Asset *a = app->asset("input");
    if (app->shader.ID) {
      W->gd->ActiveTexture(0);
      W->gd->BindTexture(GraphicsDevice::Texture2D, a->tex.ID);
      W->gd->UseShader(&app->shader);

      // mandelbox params
      float par[20][3] = { 0.25, -1.77 };
      app->shader.SetUniform1f("xres", W->gl_w);
      app->shader.SetUniform3fv("par", sizeofarray(par), &par[0][0]);
      app->shader.SetUniform1f("fov_x", FLAGS_field_of_view);
      app->shader.SetUniform1f("fov_y", FLAGS_field_of_view);
      app->shader.SetUniform1f("min_dist", .000001);
      app->shader.SetUniform1i("max_steps", 128);
      app->shader.SetUniform1i("iters", 14);
      app->shader.SetUniform1i("color_iters", 10);
      app->shader.SetUniform1f("ao_eps", .0005);
      app->shader.SetUniform1f("ao_strength", .1);
      app->shader.SetUniform1f("glow_strength", .5);
      app->shader.SetUniform1f("dist_to_color", .2);
      app->shader.SetUniform1f("x_scale", 1);
      app->shader.SetUniform1f("x_offset", 0);
      app->shader.SetUniform1f("y_scale", 1);
      app->shader.SetUniform1f("y_offset", 0);

      v3 up = scene.cam.up, ort = scene.cam.ort, pos = scene.cam.pos;
      v3 right = v3::Cross(ort, up);
      float m[16] = { right.x, right.y, right.z, 0,
                      up.x,    up.y,    up.z,    0,
                      ort.x,   ort.y,   ort.z,   0,
                      pos.x,   pos.y,   pos.z,   0 };
      W->gd->LoadIdentity();
      W->gd->Mult(m);

      glShadertoyShaderWindows(W->gd, &app->shader, Color::black,
                               Box(-W->gl_w/2, -W->gl_h/2, W->gl_w, W->gl_h));
    } else {
      scene.cam.Look(W->gd);
      scene.Draw(W->gd, &app->asset.vec);
    }
  }

  void Frame2D(LFL::Window *W, unsigned clicks, int flag) {
    GraphicsContext gc(W->gd);
    Asset *a = app->asset("input");
    if (app->shader.ID) {
      W->gd->ActiveTexture(0);
      W->gd->BindTexture(GraphicsDevice::Texture2D, a->tex.ID);
      W->gd->UseShader(&app->shader);
      glShadertoyShaderWindows(W->gd, &app->shader, Color::black, Box(W->gl_w, W->gl_h));
    } else {
      W->gd->EnableLayering();
      a->tex.Draw(&gc, W->Box());
    }
  }

  int Frame(LFL::Window *W, unsigned clicks, int flag) {
    if (FLAGS_input_3D) Frame3D(W, clicks, flag);
    W->gd->DrawMode(DrawMode::_2D);
    if (!FLAGS_input_3D) Frame2D(W, clicks, flag);
    W->DrawDialogs();
    return 0;
  }
};

void MyApp::OnWindowInit(Window *W) {
  W->gl_w = 420;
  W->gl_h = 380;
  W->caption = "Image";
}

void MyApp::OnWindowStart(Window *W) {
  MyView *view = W->AddView(make_unique<MyView>(W));
  W->frame_cb = bind(&MyView::Frame, view, _1, _2, _3);
  W->shell = make_unique<Shell>(W);

  BindMap *binds = W->AddInputController(make_unique<BindMap>());
  binds->Add(Key::Backquote, Bind::CB(bind([&](){ W->shell->console(vector<string>()); })));
  binds->Add(Key::Quote,     Bind::CB(bind([&](){ W->shell->console(vector<string>()); })));
  binds->Add(Key::Escape,    Bind::CB(bind(&Shell::quit,            W->shell.get(), vector<string>())));
  binds->Add(Key::Return,    Bind::CB(bind(&Shell::grabmode,        W->shell.get(), vector<string>())));
  binds->Add(Key::LeftShift, Bind::TimeCB(bind(&Entity::RollLeft,   &view->scene.cam, _1)));
  binds->Add(Key::Space,     Bind::TimeCB(bind(&Entity::RollRight,  &view->scene.cam, _1)));
  binds->Add('w',            Bind::TimeCB(bind(&Entity::MoveFwd,    &view->scene.cam, _1)));
  binds->Add('s',            Bind::TimeCB(bind(&Entity::MoveRev,    &view->scene.cam, _1)));
  binds->Add('a',            Bind::TimeCB(bind(&Entity::MoveLeft,   &view->scene.cam, _1)));
  binds->Add('d',            Bind::TimeCB(bind(&Entity::MoveRight,  &view->scene.cam, _1)));
  binds->Add('q',            Bind::TimeCB(bind(&Entity::MoveDown,   &view->scene.cam, _1)));
  binds->Add('e',            Bind::TimeCB(bind(&Entity::MoveUp,     &view->scene.cam, _1)));
}

}; // namespace LFL
using namespace LFL;

extern "C" LFApp *MyAppCreate(int argc, const char* const* argv) {
  FLAGS_near_plane = 0.1;
  FLAGS_enable_video = FLAGS_enable_input = true;
  app = make_unique<MyApp>(argc, argv).release();
  app->focused = CreateWindow(app).release();
  app->window_start_cb = bind(&MyApp::OnWindowStart, app, _1);
  app->window_init_cb = bind(&MyApp::OnWindowInit, app, _1);
  app->window_init_cb(app->focused);
  return app;
}

extern "C" int MyAppMain() {
  if (app->Create(__FILE__)) return -1;
  if (app->Init()) return -1;

  // app->asset.Add(name,  texture,     scale, translate, rotate, geometry
  app->asset.Add(app, "axis",   "",          0,     0,         0,      nullptr,                  nullptr, 0, 0, Asset::DrawCB(bind(&glAxis, _1, _2, _3)));
  app->asset.Add(app, "grid",   "",          0,     0,         0,      Grid::Grid3D().release(), nullptr, 0, 0);
  app->asset.Add(app, "input",  "",          0,     0,         0,      nullptr,                  nullptr, 0, 0);
  app->asset.Load();

  // app->soundasset.Add(name, filename,   ringbuf, channels, sample_rate, seconds);
  app->soundasset.Add(app, "draw",  "Draw.wav", nullptr, 0,        0,           0      );
  app->soundasset.Load();

  app->scheduler.AddMainWaitKeyboard(app->focused);
  app->scheduler.AddMainWaitMouse(app->focused);
  app->StartNewWindow(app->focused);
  MyView *view = app->focused->GetView<MyView>(0);

  if (!FLAGS_make_png_atlas.empty()) {
    FLAGS_atlas_dump=1;
    vector<string> png;
    DirectoryIter d(FLAGS_make_png_atlas, 0, 0, ".png");
    for (const char *fn = d.Next(); fn; fn = d.Next()) png.push_back(FLAGS_make_png_atlas + fn);
    AtlasFontEngine::MakeFromPNGFiles(app->fonts.get(), "png_atlas", png, point(FLAGS_make_png_atlas_width, FLAGS_make_png_atlas_height));
  }

  if (!FLAGS_split_png_atlas.empty()) {
    app->fonts->atlas_engine.get(app->fonts.get())->Init(FontDesc(FLAGS_split_png_atlas, "", 0, Color::black, Color::clear, 0));
    Font *font = app->fonts->Get(app->focused->gl_h, FLAGS_split_png_atlas, "", 0, Color::black, Color::clear, 0);
    CHECK(font);

    map<v4, int> glyph_index;
    for (auto b = font->glyph->table.begin(), e = font->glyph->table.end(), i = b; i != e; ++i) {
      CHECK(i->tex.width > 0 && i->tex.height > 0 && i->advance > 0);
      glyph_index[v4(i->tex.coord)] = i->id;
    }
    for (auto b = font->glyph->index.begin(), e = font->glyph->index.end(), i = b; i != e; ++i) {
      CHECK(i->second.tex.width > 0 && i->second.tex.height > 0 && i->second.advance > 0);
      glyph_index[v4(i->second.tex.coord)] = i->second.id;
    }

    map<int, v4> glyphs;
    for (auto i = glyph_index.begin(); i != glyph_index.end(); ++i) glyphs[i->second] = i->first;

    string outdir = StrCat(app->assetdir, FLAGS_split_png_atlas);
    LocalFile::mkdir(outdir, 0755);
    string atlas_png_fn = StrCat(app->assetdir, FLAGS_split_png_atlas, ",0,0,0,0,0.0000.png");
    AtlasFontEngine::SplitIntoPNGFiles(app->focused, atlas_png_fn, glyphs, outdir + LocalFile::Slash);
  }

  if (!FLAGS_filter_png_atlas.empty()) {
    if (unique_ptr<Font> f = AtlasFontEngine::OpenAtlas(app->fonts.get(), FontDesc(FLAGS_filter_png_atlas, "", 0, Color::white))) {
      AtlasFontEngine::WriteGlyphFile(app, FLAGS_filter_png_atlas, f.get());
      INFO("filtered ", FLAGS_filter_png_atlas);
    }
  }

  if (FLAGS_input.empty()) FATAL("no input supplied");
  Asset *asset_input = app->asset("input");
  unique_ptr<File> input_file(make_unique<LocalFile>(FLAGS_input, "r"));

  if (!FLAGS_shader.empty()) {
    string vertex_shader = LocalFile::FileContents(StrCat(app->assetdir, FLAGS_input_3D ? "" : "lfapp_", "vertex.glsl"));
    string fragment_shader = LocalFile::FileContents(FLAGS_shader);
    Shader::Create(app, "my_shader", vertex_shader, fragment_shader, ShaderDefines(0,0,1,0), &app->shader);
  }

  if (SuffixMatch(FLAGS_input, ".obj", false)) {
    FLAGS_input_3D = true;
    asset_input->cb = bind(&MyView::DrawInput3D, view, _1, _2, _3);
    asset_input->scale = FLAGS_input_scale;
    asset_input->geometry = Geometry::LoadOBJ(input_file.get()).release();
    view->scene.Add(make_unique<Entity>("axis",  app->asset("axis")));
    view->scene.Add(make_unique<Entity>("grid",  app->asset("grid")));
    view->scene.Add(make_unique<Entity>("input", asset_input));

    if (!FLAGS_output.empty()) {
      set<int> filter_prims;
      bool filter_invert = FLAGS_input_filter == "notprims", filter = false;
      if (filter_invert || FLAGS_input_filter == "prims")    filter = true;
      if (filter) Split(FLAGS_input_prims, isint2<',', ' '>, &filter_prims);
      string out = Geometry::ExportOBJ(asset_input->geometry, filter ? &filter_prims : 0, filter_invert);
      int ret = LocalFile::WriteFile(FLAGS_output, out);
      INFO("write ", FLAGS_output, " = ", ret);
    }
  } else {
    FLAGS_draw_grid = true;
    Texture pb(app);
    auto loader = app->asset_loader->default_video_loader;
    auto handle = loader->LoadVideoFile(move(input_file));
    loader->LoadVideo(handle, &asset_input->tex);
    pb.AssignBuffer(&asset_input->tex, true);

    if (pb.width && pb.height) app->focused->Reshape(FLAGS_input_scale ? pb.width *FLAGS_input_scale : pb.width,
                                                     FLAGS_input_scale ? pb.height*FLAGS_input_scale : pb.height);
    INFO("input dim = (", pb.width, ", ", pb.height, ") pf=", pb.pf);

    if (FLAGS_input_filter == "dark2alpha") {
      for (int i=0; i<pb.height; i++)
        for (int j=0; j<pb.width; j++) {
          int ind = (i*pb.width + j) * Pixel::Size(pb.pf);
          unsigned char *b = pb.buf + ind;
          float dark = b[0]; // + b[1] + b[2] / 3.0;
          // if (dark < 256*1/3.0) b[3] = 0;
          b[3] = dark;
        }
    }

    if (FLAGS_paste_image.size()) {
      Texture paste_tex(app), resample_tex(app), *in_tex = &paste_tex;
      auto loader = app->asset_loader->default_video_loader;
      auto handle = loader->LoadVideoFile(make_unique<LocalFile>(FLAGS_paste_image, "r"));
      loader->LoadVideo(handle, &paste_tex, 0);
      Box paste_box(paste_tex.width, paste_tex.height);
      if (PrefixMatch(FLAGS_paste_to, "rect:")) paste_box = Box::FromString(FLAGS_paste_to.substr(5));
      if (paste_tex.width != paste_box.w || paste_tex.height != paste_box.h) {
        in_tex = &resample_tex;
        resample_tex.Resize(paste_box.w, paste_box.h, pb.pf, Texture::Flag::CreateBuf);
        unique_ptr<VideoResamplerInterface> conv(CreateVideoResampler());
        conv->Open(paste_tex.width, paste_tex.height, paste_tex.pf,
                   paste_box.w, paste_box.h, resample_tex.pf);
        conv->Resample(paste_tex.buf, paste_tex.LineSize(), resample_tex.buf, resample_tex.LineSize(), 0, 0);
      }
      paste_box.w = max(0, min(pb.width,  paste_box.x + paste_box.w) - paste_box.x);
      paste_box.h = max(0, min(pb.height, paste_box.y + paste_box.h) - paste_box.y);
      SimpleVideoResampler::Blit(in_tex->buf, pb.buf, paste_box.w, paste_box.h,
                                 in_tex->pf, in_tex->LineSize(), 0, 0,
                                 pb.pf, pb.LineSize(), paste_box.x, paste_box.y);
    }

    if (!FLAGS_output.empty()) {
      int ret = PngWriter::Write(FLAGS_output, pb);
      INFO("write ", FLAGS_output, " = ", ret);
    }
  }

  if (!FLAGS_visualize) return 0;

  // start our engine
  return app->Main();
}
