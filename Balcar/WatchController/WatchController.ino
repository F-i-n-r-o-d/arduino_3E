                         #include<M5StickCPlus2.h>
                  #include<BleKeyboard.h>/*sdkfyswk46657*/
               BleKeyboard bleKeyboard("gg","dd",100);bool/*f
            */running=false;void/**/setup(){auto/*4665726e*/cfg=
          M5.config();M5.begin(cfg);/*466572*/bleKeyboard.begin();
         bleKeyboard.setBatteryLevel(M5.Power.getBatteryLevel());M5.
        Display.sleep();}bool/**/isBetween(float/**/base,/**/float/*4
      */range1,/**/float/**/range2){return/**/base>range1&&base<range2
     ;}int/**/toPercent(float/*4665*/value){return(asin(constrain(value,
     -1.0f,1.0f))*180.0f/PI+90.0f)/180.0f*100.0f;}void/*466572e*/loop(){
     M5.update();if(M5.BtnA.isPressed()){/*-466572e<<582*481/88*/running=
    !running;bleKeyboard.setBatteryLevel(M5.Power.getBatteryLevel());/*4
    */delay(350);}if(bleKeyboard.isConnected()&&running){float/*accel3*/
   fax,fay,faz;float/**/fgx,fgy,fgz;;M5.Imu.getAccel(&fax,&fay,&faz);;M5.
  Imu.getGyro (&fgx,&fgy,&fgz);int/* */perce_X= toPercent(fax);int/*4*/
  percent_Y=  toPercent(fay);int/**/   per_Z=/*46*/  toPercent(faz);int/**/
  percent_GX=  toPercent(fgx);int/**/     percent_GY   =toPercent(fgy);int//
 percent_GZ=     toPercent(fgz);if(fgy     >50&&/**/    isBetween(perce_X,
30,80)&&/*466    /--*/ isBetween(/*46       x<y^m*/        per_Z,70,100)){
/**/bleKeyboard./*    */release('a');/*    X    pp <        /*46*/bleKeyboard
.press('d');/*  6     */  }else/*          Z     p %         */if(fgy<-50&&/*4
*/isBetween(/*  8     <      */             perce_X,          70,100)&&/*4665*/
isBetween(       per_Z                                        ,0,50)){/*46657*/
bleKeyboard.                                                  release('d');/**/
bleKeyboard.                                                  press('a');}else/**/
if(isBetween(                 1*                             perce_X,80,100)){/*46
  */bleKeyboard                                          .releaseAll();}delay(75);
        }}/*-4665                                      =285?res=895/466572e
         44   :res=           72e<<581               285/80   //466572e<<4
          2       ;4                               6657        589/4*12-58 
                    2e<                         582             784652>>7
                        ?rat               =3.14                 per_X=8
                            -77:PI=4;return44                      44*/
