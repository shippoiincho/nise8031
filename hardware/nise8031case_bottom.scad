// nise1500 case bottom

// Bottom Outer

difference() {

translate([-47,-44.5,-32]) {    
cube([94,89,32]);
}


translate([-46,-43.5,-30]) {  
cube([92,87,30]);
}

// FD connector
translate([-24,43.5,-25]) {       
cube([70,2,25]);    
}

// USB port

translate([46,0,-15]) {
cube([2,15,5]);
}


// LINE OUT

//translate([-1,25,-2]) {
//
//cube([10,6,2]);
//
//}


//translate([-41.5,-23,-10]) {
//    
//cylinder(6,1.5,1.5);
//
//}

//translate([+41.5,-23,-10]) {
//        
//cylinder(6,1.5,1.5);
//
//}
//
}


// Bottom Hole 1

translate([-41.5,-23,-30]) {

difference() {
    
cylinder(6,3.5,3.5);    
    
cylinder(6,1.5,1.5);

}
   
}

// Botom Hole 2

translate([+41.5,-23,-30]) {

difference() {
    
cylinder(6,3.5,3.5);    
    
cylinder(6,1.5,1.5);

}
   
}
