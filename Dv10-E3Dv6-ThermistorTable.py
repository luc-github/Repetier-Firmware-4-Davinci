#!/usr/bin/env python

import math, numpy as np, matplotlib.pyplot as plt


# Vref---R2---+---RT---gnd
#             |
#             +---R1---gnd
#             |
#            Vadc

class Thermistor:

  def __init__ (self, R0_Ω, T0_C, β_K, R1, R2):
    self.β_K = β_K
    self.Ri = (R1 * R2) / (R1 + R2)
    self.Kv = 4096 * R1 / (R1 + R2)
    self.const = R0_Ω / math.exp (β_K / (T0_C + 273.15))

  def resistance (self, T_C):
    return self.const * math.exp (self.β_K / (T_C + 273.15)) 

  def adc (self, T_C, RT_Ω=None):
    if RT_Ω is None: RT_Ω = self.resistance (T_C)
    return round (self.Kv * RT_Ω / (RT_Ω + self.Ri))


class DaVinci10E3Dv6 (Thermistor):

  def __init__ (self): # Semitec 104 GT-2
    Thermistor.__init__ (self, 1e5, 25, 4267, 10000, 4700)
    self.table = (
        (-20, 1127000.0),
        (-10,  620000.0),
        (  0,  353700.0),
        ( 10,  208600.0),
        ( 20,  126800.0),
        ( 30,   79360.0),
        ( 40,   50960.0),
        ( 50,   33490.0),
        ( 60,   22510.0),
        ( 70,   15440.0),
        ( 80,   10800.0),
        ( 90,    7686.0),
        (100,    5556.0),
        (110,    4082.0),
        (120,    3043.0),
        (130,    2298.0),
        (140,    1758.0),
        (150,    1360.0),
        (160,    1064.0),
        (170,     841.4),
        (180,     671.4),
        (190,     540.8),
        (200,     439.3),
        (210,     359.7),
        (220,     296.9),
        (230,     246.8),
        (240,     206.5),
        (250,     174.0),
        (260,     147.5),
        (270,     125.8),
        (280,     107.9),
        (290,     93.05),
        (300,     80.65))

  def print_map (self):
    print ('#define EXT0_TEMPSENSOR_TYPE 7')
    print (f'#define NUM_TEMPS_USERTHERMISTOR2 {len(self.table)}')
    print ('#define USER_THERMISTORTABLE2 {', end = '')
    for t, r in reversed(self.table):
      a = self.adc(t, r) 
      print (f'{{{a},{t*8}}},', end='')
    print ('}')


if __name__ == '__main__':

  t = DaVinci10E3Dv6 ()
  t.print_map()

  print ('R@-20 =', t.resistance (-20), t.adc(-20))
  print ('R@25  =', t.resistance (25),  t.adc(25))
  print ('R@300 =', t.resistance (300), t.adc(300))

  x = np.linspace (-20, 300, 33)
  y = [t.adc(i) for i in x]
  plt.plot(x, y)
  plt.grid()
  plt.show()

