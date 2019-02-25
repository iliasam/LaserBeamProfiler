# LaserBeamProfiler
Simple 1-axis DIY laser beam profiler  
It is used for measuring laser beam profile at the single axis.  
It is using "Knife Edge" method for measuring the beam profile.  
  
My profiler consists of: 
* base plate
* linear stepper motor: 8HY001Y-D1, 0.0025mm per step.  
* HIWIN linear bearing
* Blade
* Photodiode with amplifier
* A4988 module for a stepper motor 
* Blue Pill - STM32F103 board.  
  
Photodiode amplifer is connected to the PA0 pin.  
A4988 step input must be connected to the PB0 pin.  
A4988 dir input must be connected to the PB1 pin.  
A4988 enable input must be connected to the PB11 pin.  

Profiler is making 2000 steps and then send captured ADC results to the PC by USB.  
  
Photo of the device:
![Alt text](photo.jpg?raw=true "Image")  
  
PC utility interface:  
![Alt text](Screenshot.png?raw=true "Image")
