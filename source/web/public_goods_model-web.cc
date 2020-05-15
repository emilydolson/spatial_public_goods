//  This file is part of Project Name
//  Copyright (C) Michigan State University, 2017.
//  Released under the MIT Software license; see doc/LICENSE

#include "web/web.h"
#include "web/color_map.h"
#include "../public_goods_model.h"
#include "config/config_web_interface.h"
#include "tools/spatial_stats.h"

namespace UI = emp::web;

class HCAWebInterface : public UI::Animate, public HCAWorld{
  // friend class HCAWorld;
  // friend class UI::Animate;

  using color_fun_t = std::function<std::string(int)>;
  using should_draw_fun_t = std::function<bool(int)>;

  PublicGoodsConfig config;
  emp::ConfigWebUI config_ui;
  emp::Random r;
  // HCAWorld world;

  // UI::Animate anim;
  UI::Document public_good_area;
  UI::Document cell_area;
  UI::Document controls;
  UI::Document stats_area;
  UI::Canvas public_good_display;
  UI::Canvas public_good_vertical_display;
  UI::Canvas cell_display;
  // UI::Canvas clade_display;
  const double display_cell_size = 10;

  UI::Button toggle;
  UI::Style button_style;
  bool draw_cells = true;


  color_fun_t cell_color_fun;
  UI::Selector cell_color_control;
                                     
  color_fun_t age_color_fun = [this](int cell_id) {
                                        double hue = (pop[cell_id]->age/AGE_LIMIT) * 280.0;
                                        return emp::ColorHSL(hue,50,50);
                                     };

  color_fun_t producer_color_fun = [this](int cell_id) {
                                        double hue = 100 + pop[cell_id]->producer * 100;
                                        return emp::ColorHSL(hue,50,50);
                                     };


  should_draw_fun_t should_draw_cell_fun;
  should_draw_fun_t draw_if_occupied = [this](int cell_id){return IsOccupied(cell_id);};
  should_draw_fun_t always_draw = [](int cell_id){return true;};

  public:
  HCAWebInterface() : config_ui(config), public_good_area("public_good_area"), cell_area("cell_area"), controls("control_area"), stats_area("stats_area"),
    public_good_display(100, 100, "public_good_display"), public_good_vertical_display(100, 100, "public_good_vertical_display"), cell_display(100, 100, "cell_display"),
    cell_color_control("cell_color_control")
    // : anim([this](){DoFrame();}, public_good_display, cell_display) 
  {
    SetupInterface();   
  }

  void SetupInterface() {
    GetRandom().ResetSeed(config.SEED());
    Setup(config, true);

    public_good_area.SetWidth(WORLD_X * display_cell_size + WORLD_Z * display_cell_size + 10);
    public_good_display.SetSize(WORLD_X * display_cell_size, WORLD_Y * display_cell_size);
    public_good_display.Clear("black");

    public_good_vertical_display.SetSize(WORLD_Z * display_cell_size, WORLD_Y * display_cell_size);
    public_good_vertical_display.Clear("black");

    cell_area.SetWidth(WORLD_X * display_cell_size);
    cell_display.SetSize(WORLD_X * display_cell_size, WORLD_Y * display_cell_size);
    cell_display.Clear("black");

    public_good_area << "<h1 class='text-center'>PublicGood</h1>" << public_good_vertical_display << " " << public_good_display ;
    cell_area << "<h1 class='text-center'>Cells</h1>" << cell_display;
    controls << "<h1 class='text-center'>Controls</h1>";
    stats_area << "<h1 class='text-center'>Statistics</h1>";

    cell_color_fun = producer_color_fun;
    should_draw_cell_fun = draw_if_occupied;

    cell_color_control.SetOption("Age", 
                                 [this](){
                                     cell_color_fun = age_color_fun;
                                     should_draw_cell_fun = draw_if_occupied;
                                     RedrawCells();
                                 }, 1);

    cell_color_control.SetOption("Producer", 
                                 [this](){
                                     cell_color_fun = producer_color_fun;
                                     should_draw_cell_fun = draw_if_occupied;
                                     RedrawCells();
                                 }, 0);


    toggle = GetToggleButton("but_toggle");
    button_style.AddClass("btn");
    button_style.AddClass("btn-primary");
    toggle.SetCSS(button_style);

    UI::Button reset_button([this](){Reset(config, true); RedrawCells(); RedrawPublicGood();}, "Reset");
    reset_button.SetCSS(button_style);
    
    controls << toggle;
    controls << " " << reset_button << " " << cell_color_control  << "<br>";

    public_good_display.On("click", [this](int x, int y){PublicGoodClick(x, y);});;
    RedrawPublicGood();
    RedrawCells();
    
    emp::web::OnDocumentReady([](){
        // EM_ASM(d3.select("#but_toggle").classed("btn btn-primary", true););
        EM_ASM($('select').selectpicker('setStyle', 'btn-primary'););
    });

    config_ui.SetOnChangeFun([this](const std::string & val){ std::cout << "New val: " << val<<std::endl;;InitConfigs(config);});
    config_ui.ExcludeConfig("SEED");
    config_ui.ExcludeConfig("TIME_STEPS");
    config_ui.ExcludeConfig("WORLD_X");
    config_ui.ExcludeConfig("WORLD_Y");
    config_ui.ExcludeConfig("WORLD_Z");
    config_ui.ExcludeConfig("DATA_RESOLUTION");
    config_ui.Setup();
    controls << config_ui.GetDiv();

    stats_area << "<br>Time step: " << emp::web::Live( [this](){ return GetUpdate(); } );
    // stats_area << "<br>Extant taxa: " << emp::web::Live( [this](){ return systematics[0].DynamicCast<emp::Systematics<Cell, int>>()->GetNumActive(); } );
    // stats_area << "<br>Shannon diversity: " << emp::web::Live( [this](){ return systematics[0].DynamicCast<emp::Systematics<Cell, int>>()->CalcDiversity(); } );
    // stats_area << "<br>Sackin Index: " << emp::web::Live( [this](){ return systematics[0].DynamicCast<emp::Systematics<Cell, int>>()->SackinIndex(); } );
    // stats_area << "<br>Colless-Like Index: " << emp::web::Live( [this](){ return systematics[0].DynamicCast<emp::Systematics<Cell, int>>()->CollessLikeIndex(); } );
    // stats_area << "<br>Phylogenetic diversity: " << emp::web::Live( [this](){ return systematics[0].DynamicCast<emp::Systematics<Cell, int>>()->GetPhylogeneticDiversity(); } );
    // stats_area << "<br>Mean pairwise distance: " << emp::web::Live( [this](){ return systematics[0].DynamicCast<emp::Systematics<Cell, int>>()->GetMeanPairwiseDistance(); } );
    // stats_area << "<br>Variance pairwise distance: " << emp::web::Live( [this](){ return systematics[0].DynamicCast<emp::Systematics<Cell, int>>()->GetVariancePairwiseDistance(); } );
  }

  void DoFrame() {
    // std::cout << frame_count << " " << GetStepTime() << std::endl;
    UpdatePublicGood();

    if (frame_count % DIFFUSION_STEPS_PER_TIME_STEP == 0) {
      RunStep();
      RedrawPublicGood();
      if (draw_cells) {
        RedrawCells();
      }
    }

    stats_area.Redraw();
  }

  void RedrawPublicGood() {
    // public_good_display.SetSize(WORLD_X * display_cell_size, WORLD_Y * display_cell_size);
    public_good_display.Freeze();
    public_good_display.Clear("black");

    for (size_t x = 0; x < WORLD_X; x++) {
      for (size_t y = 0; y < WORLD_Y; y++) {
        double o2 = public_good->GetVal(x,y);

        o2 *= 280;
        if (o2 > 280) {
          o2 = 280;
        } else if (o2 < 0) {
          o2 = 0;
        }
        std::string color = emp::ColorHSL(o2,50,50);
        public_good_display.Rect(x*display_cell_size, y*display_cell_size, display_cell_size, display_cell_size, color, color);
      }
    }

    public_good_display.Activate();

    public_good_vertical_display.Freeze();
    public_good_vertical_display.Clear("black");

    for (size_t z = 0; z < WORLD_Z; z++) {
      for (size_t y = 0; y < WORLD_Y; y++) {
        double o2 = public_good->GetVal(WORLD_X/2,y,z);

        o2 *= 280;
        if (o2 > 280) {
          o2 = 280;
        } else if (o2 < 0) {
          o2 = 0;
        }
        std::string color = emp::ColorHSL(o2,50,50);
        public_good_vertical_display.Rect((WORLD_Z - z - 1)*display_cell_size, y*display_cell_size, display_cell_size, display_cell_size, color, color);
      }
    }
    public_good_vertical_display.Activate();
  }

  void RedrawCells(){
    // cell_display.SetSize(WORLD_X * display_cell_size, WORLD_Y * display_cell_size);
    cell_display.Freeze();
    cell_display.Clear("black");

    for (size_t x = 0; x < WORLD_X; x++) {
      for (size_t y = 0; y < WORLD_Y; y++) {
        size_t cell_id = x + y * WORLD_X;
        if (should_draw_cell_fun(cell_id)) {
          std::string color = cell_color_fun(cell_id);


          cell_display.Rect(x*display_cell_size, y*display_cell_size, display_cell_size, display_cell_size, color, color);

        }        
      }
    }

    cell_display.Activate();
  }

  void PublicGoodClick(int x, int y) { 
    // std::cout << "x: " << in_x << " y: " << in_y  <<std::endl;
    // double x = canvas.GetAdjustedX(in_x);
    // double y = canvas.GetAdjustedY(in_y);

    // UI::Canvas canvas = doc.Canvas("world_canvas");
    const double canvas_x = (double) public_good_display.GetWidth();
    const double canvas_y = (double) public_good_display.GetHeight();
    double px = ((double) x) / canvas_x;
    double py = ((double) y) / canvas_y;

    size_t pos_x = (size_t) (WORLD_X * px);
    size_t pos_y = (size_t) (WORLD_Y * py);
    // std::cout << "x: " << x << " y: " << y << "WORLD_X: " << WORLD_X << " WORLD_Y: " << WORLD_Y << " canvas_x: " << canvas_x <<" canvas_y: " << canvas_y  << " px: " << px <<  " py: " << py <<" pos_x: " << pos_x << " pos_y: " << pos_y <<std::endl;
    public_good->SetNextVal(pos_x, pos_y, 0, 1);
    public_good->SetVal(pos_x, pos_y, 0, 1);
  }
};


HCAWebInterface interface;

int main()
{
}
