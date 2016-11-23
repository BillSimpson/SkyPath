#include <pebble.h>
//
// A simple test to calculate solar and lunar elevation and azimuth
// 
// Based upon suncalc java script code.  
//
// https://github.com/mourner/suncalc/blob/master/suncalc.js
//
static Window *s_main_window;
// set up text layers for time and SZA information
static TextLayer *s_time_layer;
static TextLayer *s_sza_layer;
// set up canvas layer for drawing
static Layer *s_canvas_layer;

// Global variables
float solar_elev[] = { -14, -12, -9, -6, -3, 0, 3, 6, 8, 10, 12, 13, 14, 14, 13, 12, 10, 8, 6, 3, 0, -3, -6, -9, -12, -14 };
float graph_width, graph_height;
float curr_solar_elev;

static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  // Write the current hours and minutes into a buffer
  static char s_buffer[8];
  static char sza_buffer[10];
  
  strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ?
                                          "%H:%M" : "%I:%M", tick_time);

  // Display this time on the TextLayer
  text_layer_set_text(s_time_layer, s_buffer);
  
  // Update the solar elevation 
  snprintf(sza_buffer, 9, "SunEl %d",(int)curr_solar_elev);
  text_layer_set_text(s_sza_layer, sza_buffer);
  
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}

int hour_to_xpixel (float hour) {
  // width = 0 to 24 hours
  return (int)(hour/24 * graph_width);
}

int angle_to_ypixel (float angle) {
  // vertical = 100 degrees, with 90 on top, -10 at bottom
  return (int)((90-angle)/100 * graph_height);
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  // Custom drawing happens here!
  int i;
  GPoint start, end;
  
  // Disable antialiasing (enabled by default where available)
  // graphics_context_set_antialiased(ctx, false);
  
  // Set the line color
  graphics_context_set_stroke_color(ctx, GColorWhite);
  
  // Set the fill color
  graphics_context_set_fill_color(ctx, GColorWhite);
  
  // Generate the horizon box from 0 to -10 degrees elevation
  GRect horizon_box = GRect(hour_to_xpixel(0),angle_to_ypixel(0),
                            hour_to_xpixel(24),angle_to_ypixel(-10));
  // Draw the horizon box
  graphics_fill_rect(ctx, horizon_box, 0, GCornerNone);

  // Set the stroke width (must be an odd integer value)
  graphics_context_set_stroke_width(ctx, 1);

  for (i=0;i<24;i++) {
    start = GPoint(hour_to_xpixel(i),angle_to_ypixel(solar_elev[i]));
    end = GPoint(hour_to_xpixel(i+1),angle_to_ypixel(solar_elev[i+1]));
    // Draw a line
    graphics_draw_line(ctx, start, end);
  }
  
  // Calculate sun position
  // Get a tm structure
  time_t temp = time(NULL);
  struct tm *curr_time = localtime(&temp);
  int hour = curr_time->tm_hour;  
  float frac_hour = ((float)curr_time->tm_min)/60;
  curr_solar_elev = solar_elev[hour] + frac_hour * (solar_elev[hour+1]-solar_elev[hour]);
  float curr_elev = curr_solar_elev;
  if (curr_elev>0) {
    // Daytime (or twilight). Set the fill color white
    graphics_context_set_fill_color(ctx, GColorWhite);
  }
  else {
    // Nighttime. Set the fill color black
    graphics_context_set_fill_color(ctx, GColorBlack);
    if (curr_elev < -5) curr_elev = -5;
  }
  // make the radius 5 degrees in y coordinate
  int radius = angle_to_ypixel(-5) - angle_to_ypixel(0);
  GPoint center = GPoint(hour_to_xpixel(hour+frac_hour),angle_to_ypixel(curr_elev));
  // draw border
  graphics_draw_circle(ctx, center, radius);
  // draw fill 
  graphics_fill_circle(ctx, center, radius-1);
}

static void main_window_load(Window *window) {
  // Get information about the Window
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // create drawing canvas for data visualization -- top half of display
  s_canvas_layer = layer_create(
      GRect(0, 0, bounds.size.w, bounds.size.h/2));
  graph_width = bounds.size.w;
  graph_height = bounds.size.h/2;
  
  // Assign the custom drawing procedure
  layer_set_update_proc(s_canvas_layer, canvas_update_proc);

  // Add to Window
  layer_add_child(window_get_root_layer(window), s_canvas_layer);
  
  // Create the TextLayer with specific bounds on bottom half of display
  s_time_layer = text_layer_create(
      GRect(0, bounds.size.h/2, bounds.size.w, 50));
  
  // Improve the layout to be more like a watchface
  text_layer_set_background_color(s_time_layer, GColorBlack);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

  // Add it as a child layer to the Window's root layer
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
  
  // Create sza information layer
  s_sza_layer = text_layer_create(
    GRect(0, (bounds.size.h/2+51), bounds.size.w, 29));

  // Style the text
  text_layer_set_background_color(s_sza_layer, GColorBlack);
  text_layer_set_text_color(s_sza_layer, GColorWhite);
  text_layer_set_text_alignment(s_sza_layer, GTextAlignmentCenter);
  text_layer_set_font(s_sza_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text(s_sza_layer, "Sky Path");

  // Add it as a child layer to the Window's root layer
  layer_add_child(window_layer, text_layer_get_layer(s_sza_layer));

}

static void main_window_unload(Window *window) {
  // Destroy TextLayers
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_sza_layer);

  // Destroy canvas
  layer_destroy(s_canvas_layer);
}

static void init() {
  // Create main Window element and assign to pointer
  s_main_window = window_create();

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);
  
  // Register with TickTimerService
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  // Make sure the time is displayed from the start
  update_time();
  
  // Set the window background to the image background
  window_set_background_color(s_main_window, GColorBlack);
}

static void deinit() {
  // Destroy Window
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
