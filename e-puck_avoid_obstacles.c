#include <stdio.h>
#include <webots/motor.h>
#include <webots/robot.h>
#include <webots/light_sensor.h>
#include <webots/distance_sensor.h>
#include <math.h> // For fabs()
#include <webots/gps.h>

#define TIME_STEP 64       // Simulation time step in ms
#define MAX_SPEED 6.28
#define WALL_THRESHOLD 100.0
#define LIGHT_THRESHOLD 500.0
#define GPS_THRESHOLD 0.07 // GPS Tolerance Threshold

bool dead_end();

double clamp(double value, double min_value, double max_value) {
    if (value < min_value) {
        return min_value;
    } else if (value > max_value) {
        return max_value;
    }
    return value;
}

int main(int argc, char **argv) {
  // Initializing Webots API
  wb_robot_init();
  
  // Defines and enables motors
  WbDeviceTag left_motor = wb_robot_get_device("left wheel motor");
  WbDeviceTag right_motor = wb_robot_get_device("right wheel motor");
  wb_motor_set_position(left_motor, INFINITY);
  wb_motor_set_position(right_motor, INFINITY);
  
  // Initializes motor speed
  double light_deadend[11];
  double gps_coordinates[10][3]; // Stores up to 10 dead-end coordinates (x, y, z)
  double left_speed = MAX_SPEED;
  double right_speed = MAX_SPEED;
  int nest = 0;
  double max = 0;
  int j = 0;
  
  // Defines and enables the distance sensors
  WbDeviceTag distance_sensors[8];
  char dist_sensor_name[50];
  for (int i = 0; i < 8; i++) {
    sprintf(dist_sensor_name, "ps%d", i);
    distance_sensors[i] = wb_robot_get_device(dist_sensor_name);
    wb_distance_sensor_enable(distance_sensors[i], TIME_STEP);
  }
  
  // Defines and enables the light sensor
  WbDeviceTag light_sensor = wb_robot_get_device("ls0"); 
  wb_light_sensor_enable(light_sensor, TIME_STEP);

  // Enabling GPS sensor
  WbDeviceTag gps = wb_robot_get_device("gps");
  wb_gps_enable(gps, TIME_STEP);

  // Dead end detection variables
  int dead_end_count = 0;
  double dead_end_timer = 0;

  // Flag to stop the robot after reaching the target
  bool target_reached = false;

  // Function to check if the robot is in a dead end
  bool dead_end() {
    double front_distance = wb_distance_sensor_get_value(distance_sensors[0]);
    double current_time = wb_robot_get_time();

    // If sensor [7] detects a wall and it's the first detection or in a new time window
    if (front_distance > 100) {
      if (dead_end_count == 0 || (current_time - dead_end_timer) > 1.7) {
        // Reset timer on the first detection
        dead_end_timer = current_time;
        dead_end_count = dead_end_count + 1;
      }
    }

    // If the counter has reached 2 detections within 4 seconds, detect a dead-end
    if (dead_end_count >= 2) {
      dead_end_count = 0; // Reset for future detections
      return true;
    }

    // Resets dead end count if time interval exceeds 4 seconds without detections
    if ((current_time - dead_end_timer) > 10.0) {
      dead_end_count = 0;
    }
    
    return false;
  }
  
  // Main loop
  while (wb_robot_step(TIME_STEP) != -1) {
    // Gets the current GPS coordinates
    const double *gps_values = wb_gps_get_values(gps);

    // Reads distance sensor values for wall-following
    bool left_wall = wb_distance_sensor_get_value(distance_sensors[5]) > WALL_THRESHOLD;
    bool left_corner = wb_distance_sensor_get_value(distance_sensors[6]) > WALL_THRESHOLD;
    bool front_wall = wb_distance_sensor_get_value(distance_sensors[7]) > WALL_THRESHOLD;

    // Reads light sensor value to detect light sources
    double light_value = wb_light_sensor_get_value(light_sensor);

    // Checks for dead-end and handle it
    if (dead_end()) {
      // Stops briefly to record data or adjust if necessary
      if (nest < 10) {
        nest++;
        light_deadend[nest] = light_value;

        // Gets the GPS coordinates and store them (x, y, z)
        gps_coordinates[nest - 1][0] = gps_values[0]; // x-coordinate
        gps_coordinates[nest - 1][1] = gps_values[1]; // y-coordinate
        gps_coordinates[nest - 1][2] = gps_values[2]; // z-coordinate

        printf("\n_____________________________");
        printf("\nDead end %d.\n", nest);
        printf("Light intensity: %f\n", light_deadend[nest]);
        printf("Coordinates: \nx = %f \ny = %f \nz = %f\n", gps_values[0], gps_values[1], gps_values[2]);
      }

      if (nest == 10) {
        // Loops through the array to find the greatest light value
        for (int i = 1; i <= 10; i++) {
          if (light_deadend[i] > max) {
            max = light_deadend[i]; // Update max if a greater value is found
            j = i;
          }
        }
        printf("\n___________________________________________________________");
        printf("\nBrightest light intensity: %f found at dead end %d\n", max, j);
        printf("Position with brightest light intensity:\nx = %f \ny = %f \nz = %f\n",
               gps_coordinates[j - 1][0], gps_coordinates[j - 1][1], gps_coordinates[j - 1][2]);
        nest++;
      }

      // Checks if the robot has reached the target dead-end location
      if (fabs(gps_values[0] - gps_coordinates[j - 1][0]) < GPS_THRESHOLD &&
          fabs(gps_values[1] - gps_coordinates[j - 1][1]) < GPS_THRESHOLD &&
          fabs(gps_values[2] - gps_coordinates[j - 1][2]) < GPS_THRESHOLD) {
          printf("\n...........................................................");
          printf("\n\n|Robot at position with brightest light intensity reached!|\n");
          printf("\n...........................................................\n");
          target_reached = true;
      }
    } else {
      // Wall following logic
      if (front_wall) {
        // Turns right if there's a wall in front
        left_speed = MAX_SPEED;
        right_speed = -MAX_SPEED;
      } else {
        if (left_wall) {
          // Moves forward if there's a wall on the left
          left_speed = MAX_SPEED / 2;
          right_speed = MAX_SPEED / 2;
        } else if (left_corner) {
          // Slightly turns if a left corner is detected
          left_speed = MAX_SPEED;
          right_speed = MAX_SPEED / 4;
        } else {
          // Turns left if there's no wall on the left
          left_speed = MAX_SPEED / 4;
          right_speed = MAX_SPEED;
        }
      }
    }

    // Applies the clamp function to ensure the speed stays within bounds
    left_speed = clamp(left_speed, -MAX_SPEED, MAX_SPEED);
    right_speed = clamp(right_speed, -MAX_SPEED, MAX_SPEED);

    // Sets motor speeds
    wb_motor_set_velocity(left_motor, left_speed);
    wb_motor_set_velocity(right_motor, right_speed);

    // If the target dead end is been reached, the robot stops
    if (target_reached) {
      wb_motor_set_velocity(left_motor, 0);
      wb_motor_set_velocity(right_motor, 0);
      break;
    }
  }

  // Cleanup the Webots API
  wb_robot_cleanup();

  return 0;
}
