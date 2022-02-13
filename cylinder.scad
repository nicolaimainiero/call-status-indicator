$fn=100*4;

 module h_cylinder(height,diameter){
     thickness=2;
      difference() {
        cylinder(d=diameter, h=height);
        translate([0,0,thickness/2]) {
            cylinder(d=diameter-thickness, h=height);
        }
    }
}

 module ring(height,diameter, thickness){

      difference() {
        cylinder(d=diameter+thickness, h=height);
        translate([0,0,-0.5]) {
            cylinder(d=diameter, h=1+height);
        }
    }
}

module clamp(height, diameter){
    module steg(height){
        difference(){
          cube([15, 2 , height]);
            translate([12,4,height/2]){
                rotate([90, 0, 0]){
                cylinder(d=3, h=10);
                }
            }
        }
    }

    thickness = 2;
    difference(){
      ring(height, diameter, thickness);
      translate([(diameter/2)-0.8, -2 ,0]){
        color("blue"){
          cube([thickness, 4 , height ]);
        }
      }
   }
   translate([(diameter/2), 2 ,0]){
     color("red"){
       steg(height);
     }
   }
  translate([(diameter/2), -4 ,0]){
     color("red"){
        steg(height);
     }
   }
}



// rod
module rod(){
  color("grey"){
    height=200;
    translate([0, 0, -50]){
      cylinder(d=35, h=height);
    }
  }
}

module led_tray(){
  led_outer_diameter = 52;
  led_inner_diameter= led_outer_diameter - 2;
  module hole(){
    min = -8;
    max = 8;
    y = 20;
    list1 = [ for (i = [min : 0.1 : max]) [i, 1/2*i*i] ];
    //echo (list1);
    translate([50,0,y]){
      rotate([-90,0,90]){
        linear_extrude(100){
          polygon(points = concat(list1, [[0, y]]));
        }
      }
    }
  }

  height = 120;
  color("fuchsia"){
  difference(){
    ring(height+10, led_outer_diameter, 2);
    for(i = [0:36:360]){
      rotate([0, 0, i]){
        translate([-50, 0, 0]){
          hole();
        }
      }
    }
  }
  }
}


module bottom(){

  rod_diameter=35;
  thickness=2;
  translate([0,0,3]){
    clamp(6, rod_diameter);
  }

  difference(){
    ring(5, rod_diameter, thickness);
    translate([0, -(rod_diameter+(2*thickness))/2, 0]){
      cube([(rod_diameter+thickness)/2, rod_diameter+(2*thickness), 5]);
    }
  }

  union(){
    difference(){
      color("blue"){
        linear_extrude(thickness){
          circle(d=76);
        }
      }
      translate([0,0,-1]){
        linear_extrude(thickness+2){
          circle(d=rod_diameter);
        }
      }
    }
    difference(){
      color("black"){
        difference(){
          ring(15, 75, thickness+1);
          rotate([0, 0,180]){
            translate([(75/2)-4, 0, 5.5]){
              rotate([0, 90,0]){
                color("white"){
                  cylinder(d=5, 10);
                }
              }
            }
          }
        }
      }
      color("grey"){
        translate([0,0,10]){
            ring(5, 75, (thickness + 1)/2);
        }
      }
    }
  }
}

module lampshade(){
  thickness=2;
  step = 5;
  translate([0,0,10]){
    color("white"){
      ring(step, 75, (thickness + 1)/2);
      translate([0,0,step]){
        ring(121, 75,  thickness+1);
        translate([0,0,120]){
          linear_extrude(1){
            circle(d=75);
          }
        }
      }
    }
  }
}

//rod();
//bottom();
//led_tray();
//lampshade();
rotate([0,180,0]){
  translate([0,0,-136]){
    lampshade();
  }
}

