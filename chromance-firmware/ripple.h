/*
   A dot animation that travels along rails
   (C) Voidstar Lab LLC 2021
*/

#ifndef RIPPLE_H_
#define RIPPLE_H_

#define RED 0
#define GREEN 1
#define BLUE 2

// WARNING: These slow things down enough to affect performance. Don't turn on unless you need them!
//#define DEBUG_ADVANCEMENT  // Print debug messages about ripples' movement
//#define DEBUG_RENDERING  // Print debug messages about translating logical to actual position

#include <Adafruit_DotStar.h>
#include "mapping.h"

float fmap(float x, float in_min, float in_max, float out_min, float out_max) 
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

class Ripple {
  public:
    Ripple(std::function<unsigned long ()> COLOR, std::function<int ()> SPEED, long LIFETIME, byte LENGTH, std::function<float ()> FADE)
    {
      color = COLOR;
      speed = SPEED;
      lifetime = LIFETIME;
      length = LENGTH;
      fade = FADE;
      
      life = 0;
      dead = false;

      rippleArray = new PixelInfo[length];

      for(byte i = 0; i < length; i++)
      {
        assignColor(&rippleArray[i],color());
      }
    }

    void WriteToBuffer(byte ledColors[N_SEGMENTS][N_LEDSONSEGMENT][3])
    {
      for(byte i = 0; i < length; i++)
      {
        byte *red   = &ledColors[rippleArray[i].segment][rippleArray[i].led][RED];
        byte *green = &ledColors[rippleArray[i].segment][rippleArray[i].led][GREEN];
        byte *blue  = &ledColors[rippleArray[i].segment][rippleArray[i].led][BLUE];

        *red    = min(255, *red + rippleArray[i].red);
        *green  = min(255, *green + rippleArray[i].green);
        *blue   = min(255, *blue + rippleArray[i].blue);
      }
    }
  
    bool IsDead(){return dead;}

    void Advance();

    // Helper function if one does not want to write a call back function vor every paramter
    template <typename T> 
    static std::function<T ()> Const(T a) 
    {
      return [a]() { return a; };
    }

  protected:
      byte length; //Length of the ripple in pixels, 0 means, there is no ripple 

      struct PixelInfo //Stores the information of each contained pixel
      {
        bool travelingUpwards; // Need to know if we start from top or bottom Index, false means from top, true means from 0 to ...

        byte segment;
        byte led;

        byte red;
        byte green;
        byte blue;
      };
      PixelInfo *rippleArray;

      std::function<int ()> speed; // Time between the ripple advances by one LED in milli seconds
      long lastMillis = 0;

      long lifetime; // Time before the ripple will die/ become invisible in milli seconds
      long life;
      bool dead; // Is it dead?
      
      std::function<float ()> fade; // Possibly dims the ripple out over time, might be done over function variable?

      std::function<unsigned long ()> color; //Function pointer to function returning new color on call

      byte nextPixel(byte led, bool travelingUpwards) //Advances by one led with respect to up/down movement
      {
        return led + ((travelingUpwards)? 1 : -1);
      }

      bool hasNextPixel(byte led, bool travelingUpwards) //Checks if the segment has room to advance by one led with respect to up/down movement
      {
        return  0 <= nextPixel(led, travelingUpwards) && nextPixel(led, travelingUpwards) < N_LEDSONSEGMENT;
      }

      byte nextNode(byte segment, bool travelingUpwards) //Checks in which node we run when the segment runs out with respect to up/down movement
      {
        return segmentConnections[segment][!travelingUpwards];
      }

      byte comingFromPort(byte segment, bool travelingUpwards) //Checks from which port we are running into the node. First port has number 0!
      {
        byte goingInto = nextNode(segment, travelingUpwards);

        for(int i = 0; i < N_HUBCONNECTIONS; i++)
        {
          if(nodeConnections[goingInto][i] == segment) return i;
        }

        return 0;
      }

      byte segmentAtPort(byte node, byte port) //Gives the segment connected to a specified port of a node
      {
        return nodeConnections[node][port];
      }

      void assignColor(PixelInfo *pixel, unsigned long color) //Assigns a pixel a color
      {
          pixel->red    = (color >> 16) & 0xFF;
          pixel->green  = (color >> 8) & 0xFF;
          pixel->blue   = color & 0xFF;
      }
};

class RuleRipple : public Ripple
{
  public:
    RuleRipple(std::function<unsigned long ()> COLOR, std::function<int ()> SPEED, long LIFETIME, byte LENGTH, std::function<float ()> FADE, byte RULES[N_NODES][N_HUBCONNECTIONS]) : Ripple(COLOR, SPEED, LIFETIME, LENGTH, FADE)
    {
      for(int i = 0; i < N_NODES; i++)
      {
        for(int j = 0; j < N_HUBCONNECTIONS; j++)
        {
          rules[i][j] = RULES[i][j];
        }
      }

      RuleCheck();
    }

    void RuleCheck()
    {
      for(int i = 0; i < N_NODES; i++)
      {
        byte sum = 0;
        
        for(int j = 0; j < N_HUBCONNECTIONS; j++)
        {
          sum += rules[i][j];
          if(rules[i][j] != 0 && nodeConnections[i][j] == -1)
          {
            Serial.print("Rule for non existing path at node:");
            Serial.print(i);
            Serial.print(" connection: ");
            Serial.println(j);
          }
        }

        if(sum != 100)
        {
          Serial.print("Propability sum of node:");
          Serial.print(i);
          Serial.println(" not 100, remember percentage because memory");
        }
      } 
    }

    void Start(byte startingNode)
    {
      life = 0;
      dead = false;

      bool travelingUpwards;
      rippleArray[0].segment = ruleNextSegment(startingNode, 0, &(rippleArray[0].travelingUpwards));
      rippleArray[0].led = (travelingUpwards)? 0 : 13;
    }

    void Advance()
    {
      //Do we advance
      if(millis() < lastMillis + speed()) return;
      lastMillis = millis();

      life += speed();
      if(lifetime <= life) dead = true;

      // Move the tail by one
      for(byte i = length - 1; i > 0; i--)
      {
        rippleArray[i].travelingUpwards = rippleArray[i-1].travelingUpwards;

        rippleArray[i].segment          = rippleArray[i-1].segment;
        rippleArray[i].led              = rippleArray[i-1].led;

        rippleArray[i].red              = fade() * rippleArray[i-1].red;
        rippleArray[i].green            = fade() * rippleArray[i-1].green;
        rippleArray[i].blue             = fade() * rippleArray[i-1].blue;
      }

      //Move the lead
      PixelInfo *leadingPixel = &rippleArray[0];

      assignColor(leadingPixel,color());

      if(hasNextPixel(leadingPixel->led, leadingPixel->travelingUpwards)){
        leadingPixel->led = nextPixel(leadingPixel->led, leadingPixel->travelingUpwards);
      }
      else
      {
        byte ruleForNode = nextNode(leadingPixel->segment, leadingPixel->travelingUpwards);
        byte withoutPort = comingFromPort(leadingPixel->segment, leadingPixel->travelingUpwards);
        
        bool travelingUpwards;
        byte nextSegment = ruleNextSegment(ruleForNode, withoutPort, &travelingUpwards);
        byte nextLED = (travelingUpwards)? 0 : 13;

        leadingPixel->travelingUpwards = travelingUpwards;
        leadingPixel->segment = nextSegment;
        leadingPixel->led = nextLED;
      }      
    }
  
  private:
    byte ruleNextSegment(byte ruleForNode, byte withoutPort, bool *travelingUpwards) //Chooses the port through witch we leave by chance according to the rules
    {
      byte randomValue = random(0,100 - rules[ruleForNode][withoutPort] + 1); // +1 since the upper value is not included 

      byte port;
      byte sum = 0;
      for(byte i = 0; i < N_HUBCONNECTIONS; i++)
      {
        if(i == withoutPort) i++;
        
        sum += rules[ruleForNode][i];
        if(randomValue <= sum) 
        {
          *travelingUpwards = (i == 0) || (i == 1) || (i == 5);
          return segmentAtPort(ruleForNode, i);
        } 
      }

      return 0;
    }

    byte rules[N_NODES][N_HUBCONNECTIONS]; // Represents probabilities. In the interest of memory the unit is percentage without decimal places
};

class SingleRuleRipple : public Ripple
{
  public:
    SingleRuleRipple(std::function<unsigned long ()> COLOR, std::function<int ()> SPEED, long LIFETIME, byte LENGTH, std::function<float ()> FADE, byte RULES[N_HUBCONNECTIONS]) : Ripple(COLOR, SPEED, LIFETIME, LENGTH, FADE)
    {
      for(int j = 0; j < N_HUBCONNECTIONS; j++)
      {
        rules[j] = RULES[j];
      }
    }

    // void RuleCheck() //Sounds good but what would it actually do?
    
    void Start(byte startingNode)
    {
      life = 0;
      dead = false;

      bool travelingUpwards;
      rippleArray[0].segment = ruleNextSegment(startingNode, 0, &(rippleArray[0].travelingUpwards));
      rippleArray[0].led = (travelingUpwards)? 0 : 13;
    }

    void Advance()
    {
      //Do we advance
      if(millis() < lastMillis + speed()) return;
      lastMillis = millis();

      life += speed();
      if(lifetime <= life) dead = true;

      // Move the tail by one
      for(byte i = length - 1; i > 0; i--)
      {
        rippleArray[i].travelingUpwards = rippleArray[i-1].travelingUpwards;

        rippleArray[i].segment          = rippleArray[i-1].segment;
        rippleArray[i].led              = rippleArray[i-1].led;

        rippleArray[i].red              = fade() * rippleArray[i-1].red;
        rippleArray[i].green            = fade() * rippleArray[i-1].green;
        rippleArray[i].blue             = fade() * rippleArray[i-1].blue;
      }

      //Move the lead
      PixelInfo *leadingPixel = &rippleArray[0];

      assignColor(leadingPixel,color());

      if(hasNextPixel(leadingPixel->led, leadingPixel->travelingUpwards)){
        leadingPixel->led = nextPixel(leadingPixel->led, leadingPixel->travelingUpwards);
      }
      else
      {
        byte ruleForNode = nextNode(leadingPixel->segment, leadingPixel->travelingUpwards);
        byte withoutPort = comingFromPort(leadingPixel->segment, leadingPixel->travelingUpwards);
        
        bool travelingUpwards;
        byte nextSegment = ruleNextSegment(ruleForNode, withoutPort, &travelingUpwards);
        byte nextLED = (travelingUpwards)? 0 : 13;

        leadingPixel->travelingUpwards = travelingUpwards;
        leadingPixel->segment = nextSegment;
        leadingPixel->led = nextLED;
      }      
    }
  
  private:
    bool portAvailable(byte ruleForNode, byte withoutPort, byte checkPort)  //Checks if we can leave over this port, without port can be disableb by providing value -1
    {
      return (checkPort != withoutPort && segmentAtPort(ruleForNode, checkPort) != 255);
    }

    byte ruleSumOfAvailablePorts(byte ruleForNode, byte withoutPort) // "Probability sum" of available ports
    {
      byte sumOfAvailable = 0;
      
      for(byte j = 0; j < N_HUBCONNECTIONS; j++)
      {
        byte i = (withoutPort + j) % 6; //Remember, that this has to be relative to incoming port

        if(portAvailable(ruleForNode,withoutPort,i))
        {
          sumOfAvailable += rules [j];
        }
      }

      return sumOfAvailable;
    }

    byte ruleNextSegment(byte ruleForNode, byte withoutPort, bool *travelingUpwards) //Chooses the port through witch we leave by chance according to the rules
    {
      byte sumOfAvailable = ruleSumOfAvailablePorts(ruleForNode, withoutPort);
      byte randomValue = random(1,sumOfAvailable);

      Serial.print("Node: ");
      Serial.println(ruleForNode);
      Serial.print("INcoming port: ");
      Serial.println(withoutPort);

      byte port;
      byte sum = 0;
      for(byte j = 0; j < N_HUBCONNECTIONS; j++)
      {
        byte i = (withoutPort + j) % 6; //Remember, that this has to be relative to incoming port

        if(portAvailable(ruleForNode,withoutPort,i))
        {
          sum += rules[j];

          if(randomValue < sum) 
          {
            Serial.print("Rule: ");
            Serial.println(j);
            Serial.print("Port: ");
            Serial.println(i);
            Serial.print("Port available: ");
            Serial.println(portAvailable(ruleForNode,withoutPort,i));
            Serial.println(i != withoutPort);
            Serial.println(segmentAtPort(ruleForNode, i) != 255);
            Serial.println(segmentAtPort(ruleForNode, i));
            Serial.println();

            *travelingUpwards = (i == 0) || (i == 1) || (i == 5);
            return segmentAtPort(ruleForNode, i);
          }
        } 
      }

      Serial.print("sumOfAvailable: ");
      Serial.println(sumOfAvailable);
      Serial.print("randomValue: ");
      Serial.println(randomValue);
      Serial.print("sum: ");
      Serial.println(sum);
      return 0;
    }

    byte rules[N_HUBCONNECTIONS]; // Represents probabilities. In the interest of memory the unit is percentage without decimal places
};
#endif
