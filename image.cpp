/*
 * $Id: image.cpp 1336 2014-12-08 09:29:59Z justin $
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

#include "lfapp/lfapp.h"
#include "lfapp/dom.h"
#include "lfapp/css.h"
#include "lfapp/gui.h"

using namespace LFL;

bool input_3D = false;
DEFINE_string(output, "", "Output");
DEFINE_string(input, "", "Input");
DEFINE_float(input_scale, 0, "Input scale");
DEFINE_string(input_prims, "", "Comma separated list of .obj primitives to highlight");
DEFINE_string(input_filter, "", "Filter type [dark2alpha]");
DEFINE_bool(visualize, false, "Display");
DEFINE_string(shader, "", "Apply this shader");
DEFINE_string(make_png_atlas, "", "Build PNG atlas with files in directory");
DEFINE_int(make_png_atlas_size, 256, "Build PNG atlas with this size");
DEFINE_string(split_png_atlas, "", "Split PNG atlas back into individual files");
DEFINE_string(filter_png_atlas, "", "Filter PNG atlas");

BindMap binds;
Asset::Map asset;
SoundAsset::Map soundasset;

Scene scene;
Shader MyShader;

void DrawInput3D(Asset *a, Entity *e) {
    if (!FLAGS_input_prims.empty() && a && a->geometry) {
        vector<int> highlight_input_prims;
        Split(FLAGS_input_prims, isint2<',', ' '>, &highlight_input_prims);

        screen->gd->DisableLighting();
        screen->gd->Color4f(1.0, 0.0, 0.0, 1.0);
        int vpp = GraphicsDevice::VertsPerPrimitive(a->geometry->primtype);
        for (vector<int>::const_iterator i = highlight_input_prims.begin(); i != highlight_input_prims.end(); ++i) {
            CHECK_LT(*i, a->geometry->count / vpp);
            scene.Draw(a->geometry, e, *i * vpp, vpp);
        }
        screen->gd->Color4f(1.0, 1.0, 1.0, 1.0);
    }
}

void Frame3D(LFL::Window *W, unsigned clicks, unsigned mic_samples, bool cam_sample, int flag) {
    screen->camMain->look();
    scene.Draw(&asset.vec);
}

void Frame2D(LFL::Window *W, unsigned clicks, unsigned mic_samples, bool cam_sample, int flag) {
    Asset *a = asset("input");
    Box w(0, 0, screen->width, screen->height);

    if (MyShader.ID) {
        screen->gd->ActiveTexture(0);
        screen->gd->BindTexture(GraphicsDevice::Texture2D, a->tex.ID);
        screen->gd->UseShader(&MyShader);
        MyShader.setUniform1f("xres", screen->width);

        // mandelbox params
        float par[20][3] = { 0.25, -1.77 };
        MyShader.setUniform3fv("par", sizeofarray(par), &par[0][0]);
        MyShader.setUniform1f("fov_x", FLAGS_field_of_view);
        MyShader.setUniform1f("fov_y", FLAGS_field_of_view);
        MyShader.setUniform1f("min_dist", .000001);
        MyShader.setUniform1i("max_steps", 128);
        MyShader.setUniform1i("iters", 14);
        MyShader.setUniform1i("color_iters", 10);
        MyShader.setUniform1f("ao_eps", .0005);
        MyShader.setUniform1f("ao_strength", .1);
        MyShader.setUniform1f("glow_strength", .5);
        MyShader.setUniform1f("dist_to_color", .2);
        MyShader.setUniform1f("x_scale", 1);
        MyShader.setUniform1f("x_offset", 0);
        MyShader.setUniform1f("y_scale", 1);
        MyShader.setUniform1f("y_offset", 0);

        v3 up = screen->camMain->up, ort = screen->camMain->ort, pos = screen->camMain->pos;
        v3 right = v3::cross(ort, up);
        float m[16] = { right.x, right.y, right.z, 0,
                        up.x,    up.y,    up.z,    0,
                        ort.x,   ort.y,   ort.z,   0,
                        pos.x,   pos.y,   pos.z,   0 };
        screen->gd->LoadIdentity();
        screen->gd->Mult(m);

        glTimeResolutionShaderWindows(&MyShader, Color::black, w);
    } else {
        screen->gd->EnableLayering();
        a->tex.Draw(w);
    }
}

// engine callback
// driven by lfapp_frame()
int Frame(LFL::Window *W, unsigned clicks, unsigned mic_samples, bool cam_sample, int flag) {

    if (input_3D)  Frame3D(W, clicks, mic_samples, cam_sample, flag);

    screen->gd->DrawMode(DrawMode::_2D);

    if (!input_3D) Frame2D(W, clicks, mic_samples, cam_sample, flag);

    // Press tick for console
    screen->DrawDialogs();

    return 0;
}

extern "C" int main(int argc, const char *argv[]) {

    app->logfilename = StrCat(dldir(), "image.txt");
    app->frame_cb = Frame;
    screen->width = 420;
    screen->height = 380;
    screen->caption = "Image";
    FLAGS_near_plane = 0.1;

    if (app->Create(argc, argv, __FILE__)) { app->Free(); return -1; }
    if (app->Init()) { app->Free(); return -1; }

    // asset.Add(Asset(name,  texture,     scale, translate, rotate, geometry,       0, 0, 0));
    asset.Add(Asset("axis",   "",          0,     0,         0,      0,              0, 0, 0, Asset::DrawCB(bind(&glAxis, _1, _2))));
    asset.Add(Asset("grid",   "",          0,     0,         0,      Grid::Grid3D(), 0, 0, 0));
    asset.Add(Asset("input",  "",          0,     0,         0,      0,              0, 0, 0));
    asset.Load();
    app->shell.assets = &asset;

    // soundasset.Add(SoundAsset(name, filename,   ringbuf, channels, sample_rate, seconds));
    soundasset.Add(SoundAsset("draw",  "Draw.wav", 0,       0,        0,           0      ));
    soundasset.Load();
    app->shell.soundassets = &soundasset;

//  binds.push_back(Bind(key,            callback));
    binds.push_back(Bind(Key::Backquote, Bind::CB(bind([&](){ screen->console->Toggle(); }))));
    binds.push_back(Bind(Key::Quote,     Bind::CB(bind([&](){ screen->console->Toggle(); }))));
    binds.push_back(Bind(Key::Escape,    Bind::CB(bind(&Shell::quit,            &app->shell, vector<string>()))));
    binds.push_back(Bind(Key::Return,    Bind::CB(bind(&Shell::grabmode,        &app->shell, vector<string>()))));
    binds.push_back(Bind(Key::LeftShift, Bind::TimeCB(bind(&Entity::RollLeft,   screen->camMain, _1))));
    binds.push_back(Bind(Key::Space,     Bind::TimeCB(bind(&Entity::RollRight,  screen->camMain, _1))));
    binds.push_back(Bind('w',            Bind::TimeCB(bind(&Entity::MoveFwd,    screen->camMain, _1))));
    binds.push_back(Bind('s',            Bind::TimeCB(bind(&Entity::MoveRev,    screen->camMain, _1))));
    binds.push_back(Bind('a',            Bind::TimeCB(bind(&Entity::MoveLeft,   screen->camMain, _1))));
    binds.push_back(Bind('d',            Bind::TimeCB(bind(&Entity::MoveRight,  screen->camMain, _1))));
    binds.push_back(Bind('q',            Bind::TimeCB(bind(&Entity::MoveDown,   screen->camMain, _1))));
    binds.push_back(Bind('e',            Bind::TimeCB(bind(&Entity::MoveUp,     screen->camMain, _1))));
    screen->binds = &binds;

    if (!FLAGS_make_png_atlas.empty()) {
        FLAGS_atlas_dump=1;
        vector<string> png;
        DirectoryIter d(FLAGS_make_png_atlas.c_str(), 0, 0, ".png");
        for (const char *fn = d.next(); fn; fn = d.next()) png.push_back(FLAGS_make_png_atlas + fn);
        Atlas::MakeFromPNGFiles("png_atlas", png, FLAGS_make_png_atlas_size, NULL);
    }

    if (!FLAGS_split_png_atlas.empty()) {
        Fonts::InsertAtlas     (FLAGS_split_png_atlas.c_str(), "", 0, Color::black, 0);
        Font *font = Fonts::Get(FLAGS_split_png_atlas.c_str(),     0, Color::black);
        CHECK(font);
        map<v4, int> glyph_index;
        for (int i = 255; i >= 0; i--) {
            if (!font->glyph->table[i].tex.width && !font->glyph->table[i].tex.height) continue;
            glyph_index[v4(font->glyph->table[i].tex.coord)] = i;
        }
        map<int, v4> glyphs;
        for (map<v4, int>::const_iterator i = glyph_index.begin(); i != glyph_index.end(); ++i) glyphs[i->second] = i->first;

        string outdir = StrCat(ASSETS_DIR, FLAGS_split_png_atlas);
        LocalFile::mkdir(outdir.c_str(), 0755);
        string atlas_png_fn = StrCat(ASSETS_DIR, Fonts::FontName(FLAGS_split_png_atlas.c_str(), 0, Color::black, 0), "00.png");
        Atlas::SplitIntoPNGFiles(atlas_png_fn, glyphs, outdir + LocalFile::Slash);
    }

    if (!FLAGS_filter_png_atlas.empty()) {
        if (Font *f = Font::OpenAtlas(FLAGS_filter_png_atlas, 0, Color::white, 0)) {
            Atlas::WriteGlyphFile(FLAGS_filter_png_atlas, f);
            INFO("filtered ", FLAGS_filter_png_atlas);
        }
    }

    if (FLAGS_input.empty()) FATAL("no input supplied");
    Asset *asset_input = asset("input");

    if (!FLAGS_shader.empty()) {
        string vertex_shader = LocalFile::filecontents(StrCat(ASSETS_DIR, "vertex.glsl"));
        string fragment_shader = LocalFile::filecontents(FLAGS_shader.c_str());
        Shader::create("my_shader", vertex_shader.c_str(), fragment_shader.c_str(), "", &MyShader);
    }

    if (SuffixMatch(FLAGS_input, ".obj", false)) {
        input_3D = true;
        asset_input->cb = bind(&DrawInput3D, _1, _2);
        asset_input->scale = FLAGS_input_scale;
        asset_input->geometry = Geometry::LoadOBJ(FLAGS_input);
        scene.Add(new Entity("axis",  asset("axis")));
        scene.Add(new Entity("grid",  asset("grid")));
        scene.Add(new Entity("input", asset_input));

        if (!FLAGS_output.empty()) {
            set<int> filter_prims;
            bool filter_invert = FLAGS_input_filter == "notprims", filter = false;
            if (filter_invert || FLAGS_input_filter == "prims")    filter = true;
            if (filter) Split(FLAGS_input_prims, isint2<',', ' '>, &filter_prims);
            string out = Geometry::ExportOBJ(asset_input->geometry, filter ? &filter_prims : 0, filter_invert);
            int ret = LocalFile::writefile(FLAGS_output, out);
            INFO("write ", FLAGS_output, " = ", ret);
        }
    } else {
        input_3D = false;
        FLAGS_draw_grid = true;
        Texture pb;
        app->assets.default_video_loader->load(asset_input, app->assets.default_video_loader->load_video_file(FLAGS_input.c_str()), &pb);
        if (pb.width && pb.height) screen->Reshape(FLAGS_input_scale ? pb.width *FLAGS_input_scale : pb.width,
                                                   FLAGS_input_scale ? pb.height*FLAGS_input_scale : pb.height);
        INFO("input dim = (", pb.width, ", ", pb.height, ") pf=", pb.pf);

        if (FLAGS_input_filter == "dark2alpha") {
            for (int i=0; i<pb.height; i++)
                for (int j=0; j<pb.width; j++) {
                    int ind = (i*pb.width + j) * Pixel::size(pb.pf);
                    unsigned char *b = pb.buf + ind;
                    float dark = b[0]; // + b[1] + b[2] / 3.0;
                    // if (dark < 256*1/3.0) b[3] = 0;
                    b[3] = dark;
                }
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