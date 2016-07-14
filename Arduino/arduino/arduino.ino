const int EN_R = 10;
const int EN_L = 3;
const int IN1_R = 7;
const int IN2_R = 8;
const int IN1_L = 4;
const int IN2_L = 2;

const int stop_int = 0;
const int forward_int = 1;
const int backward_int = 2;
const int right_int = 3;
const int left_int = 4;
const int right_small_int = 5;
const int left_small_int = 6;

int manual_state = stop_int;

// IR0: IR sensor Front
// IR1: IR sensor Right-Front 
// IR2: IR sensor Right-Back
// IR3: IR sensor Left-Back
// IR4: IR sensor Left-Front

int IR_latest_reading[5]; // The most recent reading, for each of the 5 IR sensors (array of 5 elements)
int IR_reading[5][10]; // 2D-array with the 10 latest readings, for each the 5 IR-sensors (a int array of int arrays)
float IR_median[5]; // Median value of the 10 latest readings, for each of the 5 IR sensors
float IR_distance[5]; // Current distance for each of the 5 IR sensors (after filtering and conversion)  

// Struct describing an ADC to distance-pair (an instance of the struct will contain an ADC value
// read with analogRead and its corresponding distance, for a specific IR sensor)
typedef struct
{
  float ADC_value;
  float distance;
} ADC_distance_pair;

// Look up table (ADC value from analogRead -> distance) for IR sensor 0 (front sensor)
// (an array of ADC_distance_pairs)
ADC_distance_pair IR0_table[] =
{
  {55, 150},
  {73, 100},
  {82, 90},
  {87, 80},
  {97, 70},
  {107, 60},
  {124, 50},
  {145, 40},
  {161, 35},
  {183, 30},
  {215, 25},
  {260, 20},
  {333, 15},
  {475, 10},
  {665, 6}
};
// Look up table (ADC value from analogRead -> distance) for IR sensor 1 (right front sensor)
// (an array of ADC_distance_pairs)
ADC_distance_pair IR1_table[] =
{
  {94, 90},
  {95, 80},
  {99, 70},
  {106, 60},
  {120, 50},
  {143, 40},
  {162, 35},
  {185, 30},
  {214, 25},
  {264, 20},
  {335, 15},
  {475, 10},
  {665, 6}
};
// Look up table (ADC value from analogRead -> distance) for IR sensor 2 (right back sensor)
// (an array of ADC_distance_pairs)
ADC_distance_pair IR2_table[] =
{
  {28, 150},
  {70, 100},
  {79, 90},
  {87, 80},
  {97, 70},
  {108, 60},
  {125, 50},
  {152, 40},
  {165, 35},
  {183, 30},
  {214, 25},
  {260, 20},
  {328, 15},
  {455, 10},
  {669, 6}
};
// Look up table (ADC value from analogRead -> distance) for IR sensor 3 (left back sensor)
// (an array of ADC_distance_pairs)
ADC_distance_pair IR3_table[] =
{
  {100, 80},
  {100, 70},
  {106, 60},
  {118, 50},
  {138, 40},
  {154, 35},
  {180, 30},
  {210, 25},
  {256, 20},
  {335, 15},
  {476, 10},
  {669, 6}
};
// Look up table (ADC value from analogRead -> distance) for IR sensor 4 (left front sensor)
// (an array of ADC_distance_pairs)
ADC_distance_pair IR4_table[] =
{
  {39, 150},
  {82, 100},
  {90, 90},
  {98, 80},
  {110, 70},
  {122, 60},
  {138, 50},
  {164, 40},
  {182, 35},
  {202, 30},
  {235, 25},
  {279, 20},
  {349, 15},
  {482, 10},
  {670, 6}
};

// Top speed: analogWrite(*some pin*, 255)
// No speed: analogWrite(*some pin*, 0)

void setup()
{
 pinMode(EN_R, OUTPUT);  
 pinMode(EN_L, OUTPUT);
 pinMode(IN1_R, OUTPUT);  
 pinMode(IN2_R, OUTPUT);
 pinMode(IN1_L, OUTPUT);
 pinMode(IN2_L, OUTPUT);
 
 Serial.begin(9600);  // baudrate = 9600 bps
}

void move_forward()
{
  // right side forward
  digitalWrite(IN1_R, LOW);
  digitalWrite(IN2_R, HIGH);
 
  // left side forward
  digitalWrite(IN1_L, HIGH);
  digitalWrite(IN2_L, LOW);
 
  analogWrite(EN_R, 200); // motor speed right
  analogWrite(EN_L, 200); // motor speed left 
}

void move_backward()
{
  // right side backward
  digitalWrite(IN1_R, HIGH);
  digitalWrite(IN2_R, LOW);
 
  // left side backward
  digitalWrite(IN1_L, LOW);
  digitalWrite(IN2_L, HIGH);
 
  analogWrite(EN_R, 200); // motor speed right
  analogWrite(EN_L, 200); // motor speed left 
}

void move_right()
{
  // right side backward
  digitalWrite(IN1_R, HIGH);
  digitalWrite(IN2_R, LOW);
 
  // left side forward
  digitalWrite(IN1_L, HIGH);
  digitalWrite(IN2_L, LOW);
 
  analogWrite(EN_R, 180); // motor speed right
  analogWrite(EN_L, 180); // motor speed left 
}

void move_right_small()
{
  // right side backward
  digitalWrite(IN1_R, HIGH);
  digitalWrite(IN2_R, LOW);
 
  // left side forward
  digitalWrite(IN1_L, HIGH);
  digitalWrite(IN2_L, LOW);
 
  analogWrite(EN_R, 170); // motor speed right
  analogWrite(EN_L, 190); // motor speed left 
}

void move_left()
{
  // right side forward
  digitalWrite(IN1_R, LOW);
  digitalWrite(IN2_R, HIGH);
 
  // left side backward
  digitalWrite(IN1_L, LOW);
  digitalWrite(IN2_L, HIGH);
 
  analogWrite(EN_R, 190); // motor speed right
  analogWrite(EN_L, 170); // motor speed left 
}

void move_left_small()
{
  // right side forward
  digitalWrite(IN1_R, LOW);
  digitalWrite(IN2_R, HIGH);
 
  // left side backward
  digitalWrite(IN1_L, LOW);
  digitalWrite(IN2_L, HIGH);
 
  analogWrite(EN_R, 180); // motor speed right
  analogWrite(EN_L, 180); // motor speed left 
}

void stop()
{
  digitalWrite(IN1_R, LOW);
  digitalWrite(IN2_R, LOW);
 
  digitalWrite(IN1_L, LOW);
  digitalWrite(IN2_L, LOW);
}

void run_manual_state()
{
  switch (manual_state)
 {
   case stop_int:
   {
     stop();
     break;
   }
   
   case forward_int:
   {
     move_forward();
     break;
   }
   
   case backward_int:
   {
     move_backward();
     break;
   }
   
   case right_int:
   {
     move_right();
     break;
   }
   
   case left_int:
   {
     move_left();
     break;
   }
   
   case right_small_int:
   {
     move_right_small();
     break;
   }
   
   case left_small_int:
   {
     move_left_small();
     break;
   }
   
 }
}

void read_serial()
{
  if (Serial.available())
  {
    int serial_input = Serial.read() - '0';
    if (serial_input == stop_int || serial_input == forward_int
        || serial_input == backward_int || serial_input == right_int
        || serial_input == left_int)
    {
      if (manual_state == forward_int && serial_input == right_int)
      {
          manual_state = right_small_int;
      }
      
      else if (manual_state == forward_int && serial_input == left_int)
      {
          manual_state = left_small_int;
      }
      
      else
      {
         manual_state = serial_input;
      }
    }
    
    else
    {
      // stop the robot if there's something wrong with the serial 
      // transmission (this shouldn't normally happen)
      manual_state = stop_int;
    }
    
  }
  
}

void read_IR_sensors()
{
    // Read analog pin 0 - 4 (A0 - A4) (analogRead returns result of 10-bit ADC, an int between 0 and 1023)
    for (int sensor_ID = 0; sensor_ID <= 4; ++sensor_ID)
    {
        IR_latest_reading[sensor_ID] = analogRead(sensor_ID);
    }
}

float get_median(int array[], int no_of_elements)
{
    float median;
    
    // Sort array using Bulle sort:
    for (int i = 0; i < no_of_elements - 1; ++i) // for no_of_elements - 1 times: (no of needed cycles in the worst case)
    {
        for (int k = 0; k < no_of_elements - (i + 1); ++k)
        {
            if (array[k] > array[k + 1])
            {
                // Swap positions:
                int temp_container = array[k];
                array[k] = array[k + 1];
                array[k + 1] = temp_container;
            }
        }
    }
    
    // Get median value (two different cases depending on whether even no of elements or not)
    if (no_of_elements % 2 != 0) // if ODD no of elements: (if the remainder when dividing with 2 is non-zero)
    {
        int median_index = no_of_elements/2 - (no_of_elements/2 % 2); // median_index = floor(no_of_elements/2)
        median = array[median_index];
    }
    else // if EVEN no of elements: (if there is no remainder when dividing with 2)
    {
        int median_index = no_of_elements/2;
        median = (array[median_index] + array[median_index - 1])/2.f; // "2.f" makes sure we don't do integer division (which eg would give ous 5 instead of 5.5)
    }

    return median;
}

void filter_IR_values()
{
    for (int sensor_ID = 0; sensor_ID <= 4; ++sensor_ID) // for IR0 to IR4:
    {
        // Shift out the oldest reading:
        for (int reading_index = 9; reading_index >= 1; --reading_index)
        {
            IR_reading[sensor_ID][reading_index] = IR_reading[sensor_ID][reading_index - 1];
        }
        
        // Insert the latest reading:
        IR_reading[sensor_ID][0] = IR_latest_reading[sensor_ID];
        
        // Get the median value of the 10 latest readings: (filter sensor readings)
        int size = sizeof(IR_reading[sensor_ID])/sizeof(int); // We get the number of elements (ints) in the array by dividing the size of the array (in no of bytes) with the size of one int (in no of bytes)
        int array_copy[] = {IR_reading[sensor_ID][0], IR_reading[sensor_ID][1], // Need to make a copy here, "get_median" apparently does something weird with its passed array 
                            IR_reading[sensor_ID][2], IR_reading[sensor_ID][3], 
                            IR_reading[sensor_ID][4], IR_reading[sensor_ID][5], 
                            IR_reading[sensor_ID][6], IR_reading[sensor_ID][7], 
                            IR_reading[sensor_ID][8], IR_reading[sensor_ID][9]}; 
        IR_median[sensor_ID] = get_median(array_copy, size);
    }
}

void test()
{
    Serial.println(IR_median[1]);
}

///////////////////////////////////////////////////////////////////////
// main loop
///////////////////////////////////////////////////////////////////////
void loop()
{  
    read_serial();
    run_manual_state();
    read_IR_sensors();
    filter_IR_values();
    
    test();
    
    delay(50);  // delay for loop frequency of roughly 20 Hz
}
