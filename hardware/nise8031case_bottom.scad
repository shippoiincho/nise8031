// nise8031 case bottom
// Base = top of 2nd floor
// Spacing of PCB = 20mm
// Shell wide = 2mm
// Bottom space = 5mm

// Bottom Outer

PCBW=1.8;
PCBS=20;
SPACE=5;
SHELLW=2.0;

difference() {



translate([-46-SHELLW,-43.5-SHELLW,-(PCBW*2+PCBS+SPACE+SHELLW)]) {    
cube([92+SHELLW*2,87+SHELLW*2,(PCBW*2+PCBS+SPACE+SHELLW)]);
};

{
translate([-46,-43.5,-(PCBW*2+PCBS+SPACE)]) {  
cube([92,87,(PCBW*2+PCBS+SPACE)]);
}

// FD connector
translate([-24,43.5,-(PCBW+PCBS)]) {       
cube([70,2,(PCBW+PCBS)]);    
}

// USB port

translate([46,-43.5+26,-12]) {
cube([2,14,12]);
}

translate([-46-SHELLW,-43.5+25,-(PCBW*2+PCBS+SPACE)+SPACE+PCBW]) {
cube([2,16,10]);
}

// SD card

// LINE OUT

//translate([-1,25,-2]) {
//
//cube([10,6,2]);
//
//}

// Bottom Holes

translate([-40,-37.5,-(PCBW*2+PCBS+SPACE+SHELLW)]) {
    
cylinder(5-SHELLW,3.5,3.5);    
       
}

translate([+40,-37.5,-(PCBW*2+PCBS+SPACE+SHELLW)]) {
    
cylinder(5-SHELLW,3.5,3.5);    
       
}

translate([-40,-37.5+55,-(PCBW*2+PCBS+SPACE+SHELLW)]) {
    
cylinder(5-SHELLW,3.5,3.5);    
       
}

translate([+40,-37.5+55,-(PCBW*2+PCBS+SPACE+SHELLW)]) {
    
cylinder(5-SHELLW,3.5,3.5);    
       
}

}
}

translate([-40,-37.5,-(PCBW*2+PCBS+SPACE)]) {

difference() {
    
cylinder(5,5.5,5.5);   
   {
    cylinder(5,1.5,1.5); 
    cylinder(5-SHELLW,3.5,3.5);     
   }
       
}
    
};

translate([+40,-37.5,-(PCBW*2+PCBS+SPACE)]) {

difference() {
    
cylinder(5,5.5,5.5);   
   {
    cylinder(5,1.5,1.5); 
    cylinder(5-SHELLW,3.5,3.5);     
   }
       
}
    
};

translate([-40,-37.5+55,-(PCBW*2+PCBS+SPACE)]) {

difference() {
    
cylinder(5,5.5,5.5);   
   {
    cylinder(5,1.5,1.5); 
    cylinder(5-SHELLW,3.5,3.5);     
   }
       
}
    
};

translate([+40,-37.5+55,-(PCBW*2+PCBS+SPACE)]) {

difference() {
    
cylinder(5,5.5,5.5);   
   {
    cylinder(5,1.5,1.5); 
    cylinder(5-SHELLW,3.5,3.5);     
   }
       
}
    
};