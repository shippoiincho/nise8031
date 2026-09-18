// nise8031 case upper

// Bottom Outer

PCBW=1.8;
PCBS=20;
SPACE=7.0;
SHELLW=2.0;


difference() {

translate([-46-SHELLW,-43.5-SHELLW,0]) {    
cube([92+SHELLW*2,87+SHELLW*2,(SPACE+SHELLW)]);
};


translate([-46,-43.5,0]) {  
cube([92,87,SPACE]);
};

// LCD Window

translate([-46+8,-43.5+40,SPACE]) {
    
    cube([52,16,SHELLW]);    
       
}

// RE Hole

translate([-46+76,-43.5+46,SPACE]) {
    
    cylinder(SHELLW,4.5,4.5);  
       
}

// Upper Holes

translate([-40,-37.5,SPACE]) {
    
cylinder(SHELLW,3.5,3.5);    
       
}

translate([+40,-37.5,SPACE]) {
    
cylinder(SHELLW,3.5,3.5);    
       
}

translate([-40,-37.5+55,SPACE]) {
    
cylinder(SHELLW,3.5,3.5);    
       
}

translate([+40,-37.5+55,SPACE]) {
    
cylinder(SHELLW,3.5,3.5);    
       
}

}

translate([-40,-37.5,0]) {

difference() {
    
cylinder(SPACE,5.5,5.5);   
   {
    cylinder(SHELLW,1.5,1.5);
        translate([0,0,SHELLW]) {  
            cylinder(SPACE-SHELLW,3.5,3.5);     
        }
    }       
}
    
};

translate([+40,-37.5,0]) {

difference() {
    
cylinder(SPACE,5.5,5.5);   
   {
    cylinder(SHELLW,1.5,1.5);
        translate([0,0,SHELLW]) {  
            cylinder(SPACE-SHELLW,3.5,3.5);     
        }
    }       
}
    
};

translate([-40,-37.5+55,0]) {

difference() {
 
cylinder(SPACE,5.5,5.5);   
   {
    cylinder(SHELLW,1.5,1.5);
        translate([0,0,SHELLW]) {  
            cylinder(SPACE-SHELLW,3.5,3.5);     
        }
    }       
} 
 
    
};

translate([+40,-37.5+55,0]) {

difference() {

cylinder(SPACE,5.5,5.5);   
   {
    cylinder(SHELLW,1.5,1.5);
        translate([0,0,SHELLW]) {  
            cylinder(SPACE-SHELLW,3.5,3.5);     
        }
    }       
}
    
};
