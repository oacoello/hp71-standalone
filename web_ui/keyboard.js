function sendKey(k){

fetch("/key",{

method:"POST",

headers:{
"Content-Type":"application/json"
},

body:JSON.stringify({key:k})

})

}

document.addEventListener("keydown",function(e){

sendKey(e.key)

})

document.querySelectorAll(".key").forEach(k=>{

k.onclick=function(){

sendKey(this.innerText)

}

})