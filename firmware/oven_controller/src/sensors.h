/**
 * Temperature Sensors (MAX31855)
 * Wrapper for thermocouple reading
 */

#ifndef SENSORS_H
#define SENSORS_H

// Initialize sensors
void sensors_init();

// Read temperatures and update global state
// Returns true if reading was successful
void sensors_read();

#endif // SENSORS_H
