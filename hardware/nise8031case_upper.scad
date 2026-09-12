// nise1500 case upper

// Bottom Outer

difference() {

translate([-50,-30,0]) {    
cube([100,60,20]);
}


translate([-48,-28,0]) {  
cube([96,56,18]);
}

translate([-48,-30,-2]) {       
cube([96,56,2]);
    
}

// VGA

translate([10,25,0]) {

cube([33,10,14]);

}

// LINE OUT

translate([-1,25,0]) {

cube([18,6,8]);

}

// USB 

translate([-29,25,0]) {

cube([12,10,18]);

}

// Button Hole

translate([38.5,3.5,18]) {

cylinder(2,2.5,2.5); 

}

translate([38.5,-10,18]) {

cylinder(2,2.5,2.5); 

}


// Mount Hole

translate([-41.5,-23,18]) {
    
cylinder(2,3.5,3.5);    
       
}

translate([41.5,-23,18]) {
    
cylinder(2,3.5,3.5);    
       
}


}


// Upper Hole 1

translate([-41.5,-23,0]) {

difference() {
    
cylinder(4,3.5,3.5);    
    
cylinder(4,1.5,1.5);

}
   
}

translate([-41.5,-23,0]) {

difference() {
    
cylinder(18,5.5,5.5);    
    
cylinder(18,3.5,3.5);

}
   
}



// Upper Hole 2

translate([+41.5,-23,0]) {

difference() {
    
cylinder(4,3.5,3.5);    
    
cylinder(4,1.5,1.5);

}
   
}

translate([+41.5,-23,0]) {

difference() {
    
cylinder(18,5.5,5.5);    
    
cylinder(18,3.5,3.5);

}
   
}

// Button hole

translate([38.5,3.5,13]) {

difference() {

cylinder(5,4.5,4.5); 
    
cylinder(5,2.5,2.5);     
}

}

translate([38.5,-10,13]) {

difference() {

cylinder(5,4.5,4.5);    

cylinder(5,2.5,2.5); 

}

}
