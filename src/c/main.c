#include <pebble.h>
//
// First attempt at the sunpath watchface
//
static Window *s_main_window;
// set up text layers for time and SZA information
static TextLayer *s_time_layer;
static TextLayer *s_date_layer;
// set up canvas layer for drawing
static Layer *s_canvas_layer;
// set up sun bitmaps
static GBitmap *s_bitmap_sun_rim;
static GBitmap *s_bitmap_horizon;

// Global variables
float solar_elev[] = { -14, -12, -9, -6, -3, 0, 3, 6, 8, 10, 12, 13, 14, 14, 13, 12, 10, 8, 6, 3, 0, -3, -6, -9, -12, -14 };
float lunar_elev[] = { 0, 9, 18, 24, 30, 36, 39, 42, 43, 39, 36, 30, 24, 18, 9, 0, -3, -6, -9, -12, -14, -14, -12, -9, -6, -3 };
float lunar_azi_hour[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24};
float graph_width, graph_height;
float curr_solar_elev;

static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  // Write the current hours and minutes into a buffer
  static char s_buffer[8];
  static char s_date_buffer[12];
  
  strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ?
                                          "%H:%M" : "%I:%M", tick_time);

  // Display this time on the TextLayer
  text_layer_set_text(s_time_layer, s_buffer);
  
  // Update the date text  
  strftime(s_date_buffer, sizeof(s_date_buffer), "%Y-%b-%e", tick_time);
  text_layer_set_text(s_date_layer, s_date_buffer);
  
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}

int hour_to_xpixel (float hour) {
  // width = 0 to 24 hours
  return (int)(hour/24 * graph_width);
}

int angle_to_ypixel (float angle) {
  // vertical = 110 degrees, with 90 on top, -20 at bottom
  return (int)((90-angle)/110 * graph_height);
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  // Custom drawing happens here!
  int i;
  GPoint dot;
  
  // Disable antialiasing (enabled by default where available)
  // graphics_context_set_antialiased(ctx, false);
  
  // Set the line color
  graphics_context_set_stroke_color(ctx, GColorWhite);
  // Set the stroke width (must be an odd integer value)
  graphics_context_set_stroke_width(ctx, 1);
  // Set the fill color
  graphics_context_set_fill_color(ctx, GColorWhite);
  // Set the compositing mode (GCompOpSet is required for transparency)
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  
  // Generate the horizon 
  GRect horizon_box = GRect(hour_to_xpixel(0),angle_to_ypixel(0),hour_to_xpixel(24),7);
  // Draw the horizon box
  graphics_draw_bitmap_in_rect(ctx, s_bitmap_horizon, horizon_box);

  // Draw solar path
  for (i=0;i<24;i++) {
    dot = GPoint(hour_to_xpixel(i),angle_to_ypixel(solar_elev[i]));
    if (solar_elev[i]>0) graphics_draw_pixel(ctx, dot);
  }
  // Draw lunar path
    for (i=0;i<24;i++) {
    dot = GPoint(hour_to_xpixel(lunar_azi_hour[i]),angle_to_ypixel(lunar_elev[i]));
    if (lunar_elev[i]>0) graphics_draw_pixel(ctx, dot);
  }
  
  // Calculate sun position
  // Get a tm structure
  time_t temp = time(NULL);
  struct tm *curr_time = localtime(&temp);
  int hour = curr_time->tm_hour;  
  float frac_hour = ((float)curr_time->tm_min)/60;
  curr_solar_elev = solar_elev[hour] + frac_hour * (solar_elev[hour+1]-solar_elev[hour]);
  float curr_elev = curr_solar_elev;
 
  // If sun is too low, stop lowering its position
  if (curr_elev < -9) curr_elev = -9;
  // Get the location to place the sun
  GRect bitmap_placed = GRect(hour_to_xpixel(hour+frac_hour)-7,angle_to_ypixel(curr_elev)-6,15,13);
  // Draw the image
  graphics_draw_bitmap_in_rect(ctx, s_bitmap_sun_rim, bitmap_placed);
}

static void main_window_load(Window *window) {
  // Get information about the Window
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // create drawing canvas for data visualization -- top 40% of display
  s_canvas_layer = layer_create(
      GRect(0, 0, bounds.size.w, bounds.size.h*0.4));
  graph_width = bounds.size.w;
  graph_height = bounds.size.h*0.4;
  
  // Assign the custom drawing procedure
  layer_set_update_proc(s_canvas_layer, canvas_update_proc);

  // Add to Window
  layer_add_child(window_get_root_layer(window), s_canvas_layer);
  
  // Create the TextLayer with specific bounds on bottom half of display
  s_time_layer = text_layer_create(
      GRect(0, bounds.size.h*0.4, bounds.size.w, 45));
  
  // Improve the layout to be more like a watchface
  text_layer_set_background_color(s_time_layer, GColorBlack);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

  // Add it as a child layer to the Window's root layer
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
  
  // Create date information layer
  s_date_layer = text_layer_create(
    GRect(0, (bounds.size.h*0.4+45), bounds.size.w, 25));

  // Style the text
  text_layer_set_background_color(s_date_layer, GColorBlack);
  text_layer_set_text_color(s_date_layer, GColorWhite);
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text(s_date_layer, "Date");

  // Add it as a child layer to the Window's root layer
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));
}

static void main_window_unload(Window *window) {
  // Destroy TextLayers
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_date_layer);

  // Destroy canvas
  layer_destroy(s_canvas_layer);
  
  // Destroy the image data
  gbitmap_destroy(s_bitmap_sun_rim);
  gbitmap_destroy(s_bitmap_horizon);
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
  
  // load bitmaps
  s_bitmap_sun_rim = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SUN_RIM);
  s_bitmap_horizon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_HORIZON);

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
