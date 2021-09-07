#include <TensorFlowLite.h>

/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

/*
   Modified 2021-01-20 and 2021-02-16, Rich Freedman, Chariot Solutions, Inc. (https://chariotsolutions.com)
   for ARM (https://arm.com)

   - Make the code work with the Arduino Portenta H7 board and Portenta Vision Shield
     - Get image from camera at 320x240 (since it's the only size that works reliably right now)
     - Crop 320x240 image to square (240x240)
     - Scale 240x240 image to 96x96 as expected by TensorFlow

   - Optionally write images to the SD Card (kinda broken - often crashes after a few writes)
   - Optionally HTTP Post images to a server for display
   - Optionally log non-errors ("info") to the serial console
   - Update to work with Arduino/Mbed library version 1.3.2
*/


#include <SPI.h>
#include <TensorFlowLite.h>
#include <Adafruit_NeoPixel.h>

#include "main_functions.h"

#include "detection_responder.h"
#include "image_provider.h"
#include "model_settings.h"
#include "person_detect_model_data.h"
#include "tensorflow/lite/micro/kernels/micro_ops.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/version.h"

#include "epd1in02d_V2.h"
#include "ImageCropper.h"
#include "ImageScaler.h"
#include "ImageDither.h"
#include "imagedata.h"
#include "epdpaint.h"
#include "CameraToEPD.h"

#define lamp1 3
#define lamp2 4
#define lamp3 5

// Which pin on the Arduino is connected to the NeoPixels?
// On a Trinket or Gemma we suggest changing this to 1:
#define LED_PIN     0

// How many NeoPixels are attached to the Arduino?
#define LED_COUNT  16

// NeoPixel brightness, 0 (min) to 255 (max)
#define BRIGHTNESS 50 // Set BRIGHTNESS to about 1/5 (max = 255)

int image_count = 0; // affects tensor_arena alignment, but doesn't behave properly inside namespace
Epd epd;

unsigned char screenBuffer[1280]={0};
Paint paint(screenBuffer, 80, 128);

// Declare our NeoPixel strip object:
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRBW + NEO_KHZ800);

// Globals, used for compatibility with Arduino-style sketches.
namespace {

    /** Declare no variables before this, so as not to affect the tensor_arena alignment **/
    // tensor_arena - An area of memory to use for input, output, and intermediate arrays.
    // Requires 16 byte alignment - adjust tensor_arena_alignment_shift as necessary if you get an alignment warning at runtime
    constexpr int kTensorArenaSize = 93 * 1024;
    constexpr size_t tensor_arena_alignment_shift = 4;
    constexpr size_t tensor_arena_padded_size = kTensorArenaSize + tensor_arena_alignment_shift;
    static uint8_t tensor_arena[tensor_arena_padded_size];
    uint8_t *p_tensor_arena = tensor_arena + tensor_arena_alignment_shift;

    tflite::ErrorReporter* error_reporter = nullptr;
    const tflite::Model* model = nullptr;
    tflite::MicroInterpreter* interpreter = nullptr;
    TfLiteTensor* input = nullptr;

    constexpr bool write_serial_info = true; // controls "info" output only - errors will always be written

    constexpr int pd_large_image_width = 320;
    constexpr int pd_large_image_height = 240;
    constexpr int pd_large_image_size = (pd_large_image_width * pd_large_image_height);
    constexpr int pd_cropped_dimension = 240;
    constexpr int pd_cropped_size = pd_cropped_dimension * pd_cropped_dimension;
    constexpr int screen_image_width = EPD_WIDTH;
    constexpr int screen_image_height = EPD_HEIGHT;
    constexpr int screen_size = screen_image_width * screen_image_height;

    uint8_t largeImage[pd_large_image_size];
    uint8_t croppedImage[pd_cropped_size];
    uint8_t scaledImage[kMaxImageSize];
    uint8_t screenImage[screen_size];

    ImageCropper image_cropper;
    ImageScaler image_scaler;
    ImageDither image_dither;
    CameraToEPD image_convertor;
}  // namespace

void setup()
{
  epd.Reset();
  epd.Init(FULL);
  epd.Clear();

  delay(5000);

  pinMode(lamp1, OUTPUT);
  pinMode(lamp2, OUTPUT);
  pinMode(lamp3, OUTPUT);

  strip.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.show();            // Turn OFF all pixels ASAP
  strip.setBrightness(BRIGHTNESS);

  lamptest();

  delay(500);
  epd.Display(IMAGE_DATA);

  Serial.begin(115200);
  while (!Serial);

  Serial.println("Element14 Presents - Wizard Themed Booknook\n");

  // Set up logging. Google style is to avoid globals or statics because of
  // lifetime uncertainty, but since this has a trivial destructor it's okay.
  // NOLINTNEXTLINE(runtime-global-variables)
  static tflite::MicroErrorReporter micro_error_reporter;
  error_reporter = &micro_error_reporter;

  // Map the model into a usable data structure. This doesn't involve any
  // copying or parsing, it's a very lightweight operation.
  model = tflite::GetModel(g_person_detect_model_data);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    TF_LITE_REPORT_ERROR(error_reporter,
     "Model provided is schema version %d not equal "
     "to supported version %d.", model->version(), TFLITE_SCHEMA_VERSION
    );
    return;
  }

  // Pull in only the operation implementations we need.
  // This relies on a complete list of all the ops needed by this graph.
  // An easier approach is to just use the AllOpsResolver, but this will
  // incur some penalty in code space for op implementations that are not
  // needed by this graph.
  //
  // NOLINTNEXTLINE(runtime-global-variables)
  static tflite::MicroMutableOpResolver<3> micro_op_resolver;
  micro_op_resolver.AddDepthwiseConv2D();
  micro_op_resolver.AddConv2D();
  micro_op_resolver.AddAveragePool2D();

  // Build an interpreter to run the model with.
  static tflite::MicroInterpreter static_interpreter(model, micro_op_resolver, p_tensor_arena, kTensorArenaSize, error_reporter);
  
  interpreter = &static_interpreter;

  // Allocate memory from the tensor_arena for the model's tensors.
  TfLiteStatus allocate_status = interpreter->AllocateTensors();
  if (allocate_status != kTfLiteOk) {
    TF_LITE_REPORT_ERROR(error_reporter, "AllocateTensors() failed");
    return;
  }

  // Get information about the memory area to use for the model's input.
  input = interpreter->input(0);

  // Street lights on
    analogWrite(lamp1, 50);
    analogWrite(lamp2, 50);
    analogWrite(lamp3, 50);

}

void lamptest()
{
  // Fill along the length of the strip in various colors...
  colorWipe(strip.Color(255,   0,   0)     , 50); // Red
  colorWipe(strip.Color(  0, 255,   0)     , 50); // Green
  colorWipe(strip.Color(  0,   0, 255)     , 50); // Blue
  colorWipe(strip.Color(  0,   0,   0, 255), 50); // True white (not RGB white)

  //Street lights
  analogWrite(lamp1, 50);
  analogWrite(lamp2, 50);
  analogWrite(lamp3, 50);
}

void whiteOverRainbow(int whiteSpeed, int whiteLength) {

  if(whiteLength >= strip.numPixels()) whiteLength = strip.numPixels() - 1;

  int      head          = whiteLength - 1;
  int      tail          = 0;
  int      loops         = 3;
  int      loopNum       = 0;
  uint32_t lastTime      = millis();
  uint32_t firstPixelHue = 0;

  for(;;) { // Repeat forever (or until a 'break' or 'return')
    for(int i=0; i<strip.numPixels(); i++) {  // For each pixel in strip...
      if(((i >= tail) && (i <= head)) ||      //  If between head & tail...
         ((tail > head) && ((i >= tail) || (i <= head)))) {
        strip.setPixelColor(i, strip.Color(0, 0, 0, 255)); // Set white
      } else {                                             // else set rainbow
        int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
        strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
      }
    }

    strip.show(); // Update strip with new contents
    // There's no delay here, it just runs full-tilt until the timer and
    // counter combination below runs out.

    firstPixelHue += 40; // Advance just a little along the color wheel

    if((millis() - lastTime) > whiteSpeed) { // Time to update head/tail?
      if(++head >= strip.numPixels()) {      // Advance head, wrap around
        head = 0;
        if(++loopNum >= loops) return;
      }
      if(++tail >= strip.numPixels()) {      // Advance tail, wrap around
        tail = 0;
      }
      lastTime = millis();                   // Save time of last movement
    }
  }
}

void pulseWhite(uint8_t wait) {
  for(int j=0; j<256; j++) { // Ramp up from 0 to 255
    // Fill entire strip with white at gamma-corrected brightness level 'j':
    strip.fill(strip.Color(0, 0, 0, strip.gamma8(j)));
    strip.show();
    delay(wait);
  }

  for(int j=255; j>=0; j--) { // Ramp down from 255 to 0
    strip.fill(strip.Color(0, 0, 0, strip.gamma8(j)));
    strip.show();
    delay(wait);
  }
}

void rainbowFade2White(int wait, int rainbowLoops, int whiteLoops) {
  int fadeVal=0, fadeMax=100;

  // Hue of first pixel runs 'rainbowLoops' complete loops through the color
  // wheel. Color wheel has a range of 65536 but it's OK if we roll over, so
  // just count from 0 to rainbowLoops*65536, using steps of 256 so we
  // advance around the wheel at a decent clip.
  for(uint32_t firstPixelHue = 0; firstPixelHue < rainbowLoops*65536;
    firstPixelHue += 256) {

    for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...

      // Offset pixel hue by an amount to make one full revolution of the
      // color wheel (range of 65536) along the length of the strip
      // (strip.numPixels() steps):
      uint32_t pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());

      // strip.ColorHSV() can take 1 or 3 arguments: a hue (0 to 65535) or
      // optionally add saturation and value (brightness) (each 0 to 255).
      // Here we're using just the three-argument variant, though the
      // second value (saturation) is a constant 255.
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue, 255,
        255 * fadeVal / fadeMax)));
    }

    strip.show();
    delay(wait);

    if(firstPixelHue < 65536) {                              // First loop,
      if(fadeVal < fadeMax) fadeVal++;                       // fade in
    } else if(firstPixelHue >= ((rainbowLoops-1) * 65536)) { // Last loop,
      if(fadeVal > 0) fadeVal--;                             // fade out
    } else {
      fadeVal = fadeMax; // Interim loop, make sure fade is at max
    }
  }

  for(int k=0; k<whiteLoops; k++) {
    for(int j=0; j<256; j++) { // Ramp up 0 to 255
      // Fill entire strip with white at gamma-corrected brightness level 'j':
      strip.fill(strip.Color(0, 0, 0, strip.gamma8(j)));
      strip.show();
    }
    delay(1000); // Pause 1 second
    for(int j=255; j>=0; j--) { // Ramp down 255 to 0
      strip.fill(strip.Color(0, 0, 0, strip.gamma8(j)));
      strip.show();
    }
  }

  delay(500); // Pause 1/2 second
}

// Fill strip pixels one after another with a color. Strip is NOT cleared
// first; anything there will be covered pixel by pixel. Pass in color
// (as a single 'packed' 32-bit value, which you can get by calling
// strip.Color(red, green, blue) as shown in the loop() function above),
// and a delay time (in milliseconds) between pixels.
void colorWipe(uint32_t color, int wait) {
  for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...
    strip.setPixelColor(i, color);         //  Set pixel's color (in RAM)
    strip.show();                          //  Update strip to match
    delay(wait);                           //  Pause for a moment
  }
}

void loop()
{
  if(write_serial_info) {
      Serial.println();
      Serial.println("==============================");
      Serial.println();
      Serial.print("Getting image ");
      Serial.print(image_count);
      Serial.print(" (");
      Serial.print(pd_large_image_width);
      Serial.print("x");
      Serial.print(pd_large_image_height);
      Serial.println(") from camera");
  }

  // get 'large' image from the provider
  if (kTfLiteOk != GetImage(error_reporter, pd_large_image_width, pd_large_image_height, kNumChannels, largeImage)) {
    Serial.println("Image capture failed.");
    return;
  }

  // crop the image to the square aspect ratio that the model expects (will crop to center)
  if(write_serial_info) {
      Serial.print("Cropping image to ");
      Serial.print(pd_cropped_dimension);
      Serial.print("x");
      Serial.println(pd_cropped_dimension);
  }

  image_cropper.crop_image(largeImage, pd_large_image_width, pd_large_image_height, croppedImage, pd_cropped_dimension, pd_cropped_dimension);


  // scale the image to the size that the model expects (96x96)
  if(write_serial_info) {
      Serial.print("Scaling image to ");
      Serial.print(kNumCols);
      Serial.print("x");
      Serial.println(kNumRows);
  }
  int scale_result = image_scaler.scale_image_down(croppedImage, pd_cropped_dimension, pd_cropped_dimension, scaledImage, kNumRows, kNumCols);
  if(scale_result < 0) {
      Serial.println("Failed to scale image");
      return;
  }

  // copy the scaled image to the TF input
  if(write_serial_info) {
      Serial.println("Copying scaled image to TensorFlow model");
  }
  memcpy(input->data.uint8, scaledImage, kMaxImageSize);

  // Run the model on this input and make sure it succeeds.
  if(write_serial_info) {
      Serial.println("Invoking the TensorFlow interpreter");
  }

  if (kTfLiteOk != interpreter->Invoke()) {
    Serial.println("TensorFlow Invocation failed.");
    return;
  }

  TfLiteTensor* output = interpreter->output(0);

  // Process the inference results
  uint8_t person_score = output->data.uint8[kPersonIndex];
  uint8_t no_person_score = output->data.uint8[kNotAPersonIndex];
  RespondToDetection(error_reporter, person_score, no_person_score);

  if (person_score > no_person_score) {
    // Street lights off
    analogWrite(lamp1, 0);
    delay(800);    
    analogWrite(lamp2, 0);
    delay(800);    
    analogWrite(lamp3, 0);
    delay(1000);    

    // Flash lights
    analogWrite(lamp1, 200);
    analogWrite(lamp2, 200);
    analogWrite(lamp3, 200);    
    strip.fill(strip.Color(0, 0, 0, 200));
    strip.show();
    delay(200);
    analogWrite(lamp1, 0);
    analogWrite(lamp2, 0);
    analogWrite(lamp3, 0);  
    strip.fill(strip.Color(0, 0, 0, 0));
    strip.show();

    //Magic
    delay(1000);        
    whiteOverRainbow(75, 5);
    delay(1000);        

    Serial.println("Writing to screen");
    image_scaler.scale_image_down(croppedImage, pd_cropped_dimension, pd_cropped_dimension, screenImage, screen_image_width, screen_image_width);
    image_dither.dither_image(screenImage, screen_image_width, screen_image_width);
    //Clear the screenbuffer
    for (int i = 0; i < 1280; i++) {
      screenBuffer[i]=0;
    }
    image_convertor.Convert(screenImage,paint,screen_image_width, screen_image_width);
    paint.SetRotate(ROTATE_180);
    paint.DrawStringAt(5,20, "WANTED", &Font16, 0xff);
    epd.Reset();
    delay(500);
    epd.Display(screenBuffer);
    delay(500);

    analogWrite(lamp1, 50);
    analogWrite(lamp2, 50);
    analogWrite(lamp3, 50);

    colorWipe(strip.Color(  0,   0,   0, 255), 50); // True white (not RGB white)

    delay(15000);
  }

  image_count++;

  delay(500);
}