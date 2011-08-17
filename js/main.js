var max_img = 2;
var currnet_image = 0;

function slide_to(){
	if(currnet_image>max_img){
		currnet_image = 0;
	}
	if(currnet_image<0){
		currnet_image = max_img;
	}
	$("#slider").animate({"margin-left":"-"+(currnet_image*800)+"px"}, "slow");
}

$(document).ready(function(){
	$("#slider-left-btn").click(function(event){
		currnet_image -= 1;
		slide_to();
	});
	$("#slider-right-btn").click(function(event){
		currnet_image += 1;
		slide_to();
	});
});