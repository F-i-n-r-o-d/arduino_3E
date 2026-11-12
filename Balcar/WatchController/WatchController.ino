                                    #include<M5StickCPlus2.h>
                              #include<BleKeyboard.h>/*sdkfyswk46657*/
                           constexpr/**/bool/**/D=true,D2=true;BleKeyboard
                        /**/bK("gg","dd",100);float/**/fax,fay,faz,fgx,fgy,fgz;
                     bool/**/r=false;void/*466*/setup(){auto/*46*/cg=M5.config();
                  M5.begin(cg);M5.Display.sleep();/**/bK.begin();bK.setBatteryLevel(
                M5.Power.getBatteryLevel());if constexpr(D||D2){Serial.begin(115200);
               Serial.println("M5StickBLEkeyboard started");}}bool/**/b(float b,float/*
              */r1,float/*466572e^*/r2){return(b>r1&&b<r2);}int p(float v){return(asin(
              constrain(v,-1.0f,1.0f))*180.0f/PI+90.0f)/180.0f*100.0f;}void/**/loop(){M5
             .update();if(M5.BtnA.isPressed()){r=!r;bK.  /*466*/setBatteryLevel(M5.Power
            .getBatteryLevel(  ));delay(350);}if(/**/bK.   isConnected()&&r)/*466572e^1*/
            { M5.Imu.getAccel(   &fax,&fay,&faz); M5.Imu.      getGyro(&fgx,&fgy,&fgz);int
            pX=p(fax);int/**/pY     =p(fay);int/*46*/pZ=p(       faz);int/**/pGX=p(fgx);int
           pGY=p(fgy);int/*4*/pGZ       =p(fgz);if/*466572*/         constexpr(D)/*466572*/{
           Serial.print("X: ");/*  ~/.cfg   */Serial.print(/*  ~/.var\    */String(pX, 10));
          Serial.print("Y: ");/*  /      \     */Serial.print( /*     |     4*/String(pY,10));
          Serial.print("Z: ");/*  I      l        */Serial./*4 l      I       4*/print(String(
          pZ,10));Serial.print    ("X: -");             Serial  .print        (String(pGX,10));
          Serial.print(" Y: ");                                               Serial.print(/**/
         String(pGY,10));Serial                                                .print(" Z: -");
          Serial.print(String(                   pGZ                           ,10*1*1));Serial
           .println();}else{if                    (                            fgy>50&&b(pX,30
            ,80)&&b(pZ,70,100))                                                {if constexpr(D2)
             {Serial.print("d");                                              }else{bK.press('d'
               );bK.release('a');}               }else                        if(fgy<-50&&b(pX,70
                ,100)&&b(pZ,0,50)){                                          if constexpr(D2)/**/{
                Serial.print("a"); }                                       else{bK.press('a');/**/bK.
                release('d');}}else if                                    (b(pX,80,100)){if constexpr
                 (D2){Serial.print("\n");                                }else{bK.releaseAll();}}if(b
                  (pY,40,65)){if constexpr(                           D2){Serial.print("w");}else{bK
                    .press('w'); bK.release('s'                    );}}else if(b(pY,66,75)){if///
                      constexpr(D2){Serial.print("\0");}else{bK.release('w');bK.release('s');}}
                         else if(b(pY,80,100)){if constexpr(D2){Serial.print("s");}else{bK.
                                press('s');bK.release('w');}}}delay(75);}}