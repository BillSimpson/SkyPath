#include <pebble.h>
//
// First attempt at the skypath (sun and moon) watchface "ephemeris"
//
static Window *s_main_window;
// set up text layers for time and date and info information
static TextLayer *s_time_layer;
static TextLayer *s_date_layer;
static TextLayer *s_info_layer;
// set up canvas layer for drawing
static Layer *s_canvas_layer;
// set up sun bitmaps
static GBitmap *s_bitmap_sun_rim;
static GBitmap *s_bitmap_horizon;
static GBitmap *s_bitmap_moon;

// Global variables
static float graph_width, graph_height;
static float curr_solar_elev, curr_solar_azi, curr_lunar_elev, curr_lunar_azi;
static float solar_elev[25];
static float solar_azi[25];
static float lunar_elev[25];
static float lunar_azi[25];
static int lunar_day;
static float lat = 64.8, lng = -147;

//
// Adapted from the javascript code below to C
//
// https://github.com/mourner/suncalc/blob/master/suncalc.js
//
//
// (c) 2011-2015, Vladimir Agafonkin
// SunCalc is a JavaScript library for calculating sun/moon position and light phases.
// https://github.com/mourner/suncalc
//
// sun calculations are based on http://aa.quae.nl/en/reken/zonpositie.html formulas

// date/time constants and conversions

float pi = 3.14159268;
float rad = 3.14159268 / 180;
float daySecs = 60 * 60 * 24;
time_t J2000 = 946684800; // year 2000 in unix time
float deg_conv = 180 / 3.14159268;

float sin_pebble(float angle_radians) {
  int32_t angle_pebble = angle_radians * TRIG_MAX_ANGLE / (2*pi);
  return ((float)(sin_lookup(angle_pebble)) / (float)TRIG_MAX_RATIO);
}

float asin_pebble(float angle_radians) {
  return (angle_radians);  // use small angle formula.
}

float cos_pebble(float angle_radians) {
  int32_t angle_pebble = angle_radians * TRIG_MAX_ANGLE / (2*pi);
  return ((float)cos_lookup(angle_pebble) / (float)TRIG_MAX_RATIO);
}

float atan2_pebble(float y, float x) {
  if (x>2) APP_LOG(APP_LOG_LEVEL_DEBUG, "atan2: X too large, 100 x value = %d", (int)x);
  if (y>2) APP_LOG(APP_LOG_LEVEL_DEBUG, "atan2: Y too large, 100 x value = %d", (int)y);
  int16_t y_pebble = (int16_t) (8192 * y ); 
  int16_t x_pebble = (int16_t) (8192 * x ); 
  return (2*pi * (float)atan2_lookup(y_pebble, x_pebble) / (float)TRIG_MAX_ANGLE);
}

float fmod_pebble(float product, float divisor) {
  int32_t factor;
  float remainder;
  
  factor = (int32_t)(product/divisor);
  if (product<0) factor--;    // casting to int truncates so check if negative and decrement
  remainder = product - ((float)factor)*divisor;
  return remainder;
}

float toDays(time_t unixdate) {
  return ((float)(unixdate-J2000) / daySecs -0.5);
}

// general calculations for position

float e = 3.14159268 / 180 * 23.4397; // obliquity of the Earth

float rightAscension(float l, float b) {
  return atan2_pebble(sin_pebble(l) * cos_pebble(e) - sin_pebble(b)/cos_pebble(b) * sin_pebble(e), cos_pebble(l));
}

float declination(float l, float b) { 
  return (asin_pebble(sin_pebble(b) * cos_pebble(e) + cos_pebble(b) * sin_pebble(e) * sin_pebble(l)));
}

float azimuth(float H, float phi, float dec) {
  return (atan2_pebble(sin_pebble(H), cos_pebble(H) * sin_pebble(phi) - sin_pebble(dec)/cos_pebble(dec) * cos_pebble(phi)));
}

float altitude(float H, float phi, float dec) { 
  return (asin_pebble(sin_pebble(phi) * sin_pebble(dec) + cos_pebble(phi) * cos_pebble(dec) * cos_pebble(H))); 
}

float siderealTime(float d, float lw) { 
  return (rad * (280.16 + 360.9856235 * d) - lw);
}

// general sun calculations

float solarMeanAnomaly(float d) { 
  return (rad * (357.5291 + 0.98560028 * d)); 
}

float eclipticLongitude(float M) {
  float C = rad * (1.9148 * sin_pebble(M) + 0.02 * sin_pebble(2 * M) + 0.0003 * sin_pebble(3 * M)); // equation of center
  float P = rad * 102.9372; // perihelion of the Earth
  return (M + C + P + pi);
}

void sunCoords(float d, float *dec, float *ra) {

  float M = solarMeanAnomaly(d);
  float L = eclipticLongitude(M);

  *dec = declination(L, 0);
  *ra = rightAscension(L, 0);
}

void sunPosition(time_t unixdate, float lat, float lng, float *azi, float *alt) {
// calculates sun position for a given date and latitude/longitude

  float lw  = rad * -lng;
  float phi = rad * lat;
  float d = toDays(unixdate);

  float dec, ra;
  sunCoords(d, &dec, &ra);
  float H  = siderealTime(d, lw) - ra;

  *azi = azimuth(H, phi, dec);
  *alt = altitude(H, phi, dec);
};

// moon calculations, based on http://aa.quae.nl/en/reken/hemelpositie.html formulas

void moonCoords(float d, float *ra, float *dec) { 
// geocentric ecliptic coordinates of the moon

  float L = rad * (218.316 + 13.176396 * d); // ecliptic longitude
  float M = rad * (134.963 + 13.064993 * d); // mean anomaly
  float F = rad * (93.272 + 13.229350 * d);  // mean distance

  float l  = L + rad * 6.289 * sin_pebble(M); // longitude
  float b  = rad * 5.128 * sin_pebble(F);     // latitude

  *ra = rightAscension(l, b);
  *dec = declination(l, b);
}

void moonPosition(time_t unixdate, float lat, float lng, float *azi, float *alt) {
  float lw  = rad * -lng;
  float phi = rad * lat;
  float d = toDays(unixdate);

  float ra, dec;
  moonCoords(d, &ra, &dec);
  float H = siderealTime(d, lw) - ra;
  float h = altitude(H, phi, dec);
// formula 14.1 of "Astronomical Algorithms" 2nd edition by Jean Meeus (Willmann-Bell, Richmond) 1998.

  *azi = azimuth(H, phi, dec);
  *alt = h;
};

// Greatly simplified moon phase algorithm from
// http://jivebay.com/calculating-the-moon-phase/
// This simply takes a new moon and uses the moon cycle from there.
// The function returns an integer between 0 and 29 that is the days
// into the lunar cycle.  Thus, 0 is new moon, 15 is full moon, and 29 is new

int moonPhase(time_t unixdate) {
  float d = toDays(unixdate);
  // 2015-Jan-01 had an 11-day old moon
  // 2015-Jan-01 was 1420070400 seconds (unix time)
  // and from earlier, "d" has zero at 946684800 seconds, so 
  // the "d" value of 2015-Jan-01 = 5479, so there was a new moon 11 days earlier
  // We start with the new moon on d=5468
  return ((int)fmod_pebble((d-5468),29.5305882));
}

void sky_paths_today(float lat, float lng, float solar_elev[], float solar_azi[], float lunar_elev[], float lunar_azi[]) {
  float sol_alt, sol_azi, moon_alt, moon_azi;
  int i;
  
  // get today's date in local time  
  time_t temp = time(NULL);
  struct tm *curr_time = localtime(&temp);
  // set hour, minute, and second to 0, so that we'll calculate hourly
  curr_time->tm_min = 0;
  curr_time->tm_sec = 0;
  curr_time->tm_hour = 0;
  temp = mktime(curr_time);

  // cycle through 25 hours calculating solar and lunar parameters
  for (i=0;i<25;i++) {
    // Solar calculation
    sunPosition(temp, lat, lng, &sol_azi, &sol_alt);
    solar_elev[i] = sol_alt * deg_conv;
    solar_azi[i] = fmod_pebble(((sol_azi + pi) * deg_conv ),360);
//    APP_LOG(APP_LOG_LEVEL_DEBUG, "hour %d Solar: Elev %d  Azi %d", i, (int)solar_elev[i], (int)solar_azi[i]);

    // Lunar calculation
    moonPosition(temp, lat, lng, &moon_azi, &moon_alt);
    lunar_elev[i] = moon_alt * deg_conv;
    lunar_azi[i] = fmod_pebble(((moon_azi + pi) * deg_conv ),360);
//    APP_LOG(APP_LOG_LEVEL_DEBUG, "       Lunar: Elev %d  Azi %d", (int)lunar_elev[i], (int)lunar_azi[i]);
    
    // advance to the next hour
    temp = temp + 3600;
  }
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Re-calculated sky paths");
}

static void load_moon_image() {
  // Get a tm structure
  time_t temp = time(NULL);

  lunar_day = moonPhase(temp);
  if (lunar_day < 3)
    s_bitmap_moon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MOON1);
  if ((lunar_day < 6) && (lunar_day >= 3)) 
    s_bitmap_moon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MOON2);
  if ((lunar_day < 10) && (lunar_day >= 6))  // half moon -- 4 days
    s_bitmap_moon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MOON3);
  if ((lunar_day < 13) && (lunar_day >= 10)) 
    s_bitmap_moon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MOON4);
  if ((lunar_day < 16) && (lunar_day >= 13)) 
    s_bitmap_moon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MOON5);
  if ((lunar_day < 19) && (lunar_day >= 16)) 
    s_bitmap_moon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MOON6);
  if ((lunar_day < 23) && (lunar_day >= 19)) // half moon -- 4 days
    s_bitmap_moon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MOON7);
  if ((lunar_day < 26) && (lunar_day >= 23)) 
    s_bitmap_moon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MOON8);
  if (lunar_day >= 26) 
    s_bitmap_moon = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MOON9);
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Selected moon image for lunar day (moon phase 0-29) %d", lunar_day);
}

static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  // Write the current hours and minutes into a buffer
  static char s_buffer[8];
  static char s_date_buffer[12];
  static char s_info_buffer[15];
  
  strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ?
                                          "%k:%M" : "%l:%M", tick_time);
//  strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ?
//                                          "%H:%M" : "%I:%M", tick_time);


  // Display this time on the TextLayer
  text_layer_set_text(s_time_layer, s_buffer);
  
  // Update the date text  
  strftime(s_date_buffer, sizeof(s_date_buffer), "%Y-%b-%e", tick_time);
  text_layer_set_text(s_date_layer, s_date_buffer);
  
  // Update the info text  
  snprintf(s_info_buffer, sizeof(s_info_buffer), "S [%d|%d]", (int)curr_solar_elev, (int)curr_solar_azi);
  text_layer_set_text(s_info_layer, s_info_buffer);

  // if it is an hour boundary, re-calculate the sun and moon ephemeris
  if (tick_time->tm_min == 0) {
    psleep(5000); // wait 5 seconds after the hour before re-calculating
    // re-calculate sky paths
    sky_paths_today(lat, lng, solar_elev, solar_azi, lunar_elev, lunar_azi);
    // load a moon image (maybe a new one)
    load_moon_image();
  }
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}

int hour_to_xpixel (float hour) {
  // width = 0 to 24 hours
  return (int)(hour/24 * graph_width);
}

int angle_to_ypixel (float angle) {
  // vertical = 65 degrees, with 50 on top, -15 at bottom
  int range = (90 - lat + 23.5) * 1.35;  // full graph 135% of the potential range at that lat
  if (range>110) range = 110;
  int top = (90 - lat + 23.5) * 1.05;  // this gives a 20% buffer below the horizon.
  if (top>90) top = 90;
  return (int)((top-angle)/range * graph_height);
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  // Custom drawing happens here!
  int i;
  GPoint point1, point2;
  
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
    point1 = GPoint(hour_to_xpixel(i),angle_to_ypixel(solar_elev[i]));
    point2 = GPoint(hour_to_xpixel(i+1),angle_to_ypixel(solar_elev[i+1]));
    if ((solar_elev[i]>0)||(solar_elev[i+1]>0) ) graphics_draw_line(ctx, point1, point2);
  }
  // Draw lunar path
  float curr_azi;
  for (i=0;i<24;i++) {
    curr_azi = lunar_azi[i] - solar_azi[0];
    if (curr_azi > 360) curr_azi = curr_azi - 360;
    if (curr_azi < 0 ) curr_azi = curr_azi + 360; 
    point1 = GPoint(hour_to_xpixel(curr_azi/15),angle_to_ypixel(lunar_elev[i]));
    point2 = GPoint(hour_to_xpixel(curr_azi/15+1),angle_to_ypixel(lunar_elev[i+1]));
    if ((lunar_elev[i]>0)||(lunar_elev[i+1]>0)) graphics_draw_line(ctx, point1, point2);
  }
  
  // Calculate sun position
  // Get a tm structure
  time_t temp = time(NULL);
  struct tm *curr_time = localtime(&temp);
  int hour = curr_time->tm_hour;  
  float frac_hour = ((float)curr_time->tm_min)/60;
  curr_solar_elev = solar_elev[hour] + frac_hour * (solar_elev[hour+1]-solar_elev[hour]);
  curr_solar_azi = solar_azi[hour] + 15 * frac_hour;
  curr_solar_azi = fmod_pebble(curr_solar_azi,360);
  float curr_elev = curr_solar_elev;
 
  // If sun is too low, stop lowering its position
  if (curr_elev < -7) curr_elev = -7;
  // Get the location to place the sun
  GRect bitmap_placed = GRect(hour_to_xpixel(hour+frac_hour)-7,angle_to_ypixel(curr_elev)-6,15,13);
  // Draw the image
  graphics_draw_bitmap_in_rect(ctx, s_bitmap_sun_rim, bitmap_placed);


  curr_lunar_elev = lunar_elev[hour] + frac_hour * (lunar_elev[hour+1]-lunar_elev[hour]);
  curr_lunar_azi = lunar_azi[hour] + 15 * frac_hour;  
  curr_lunar_azi = fmod_pebble(curr_lunar_azi,360);
  curr_elev = curr_lunar_elev;

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Azi offset %d", (int)fmod_pebble(solar_azi[0],360));

  curr_azi = lunar_azi[hour] + frac_hour * 15 - solar_azi[0];  // for display purposes
  curr_azi = fmod_pebble(curr_azi,360);
  // If moon is too low, stop lowering its position
  if (curr_elev < -7) curr_elev = -7;
  // Get the location to place the moon
  GRect bitmap_moon_placed = GRect(hour_to_xpixel(curr_azi/15)-6,angle_to_ypixel(curr_elev)-6,13,13);
  // Draw the image
  graphics_draw_bitmap_in_rect(ctx, s_bitmap_moon, bitmap_moon_placed);

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

  // Create info layer
  s_info_layer = text_layer_create(
    GRect(0, (bounds.size.h*0.4+70), bounds.size.w, 25));

  // Style the text
  text_layer_set_background_color(s_info_layer, GColorBlack);
  text_layer_set_text_color(s_info_layer, GColorWhite);
  text_layer_set_text_alignment(s_info_layer, GTextAlignmentCenter);
  text_layer_set_font(s_info_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text(s_info_layer, "Sun Info");
                                     
  // Add it as a child layer to the Window's root layer
  layer_add_child(window_layer, text_layer_get_layer(s_info_layer));

}

static void main_window_unload(Window *window) {
  // Destroy TextLayers
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_date_layer);
  text_layer_destroy(s_info_layer);

  // Destroy canvas
  layer_destroy(s_canvas_layer);
  
  // Destroy the image data
  gbitmap_destroy(s_bitmap_sun_rim);
  gbitmap_destroy(s_bitmap_horizon);
  gbitmap_destroy(s_bitmap_moon);
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
  
  // get proper moon phase image
  load_moon_image();
  
  // calculate sun paths
  sky_paths_today(lat, lng, solar_elev, solar_azi, lunar_elev, lunar_azi);
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
