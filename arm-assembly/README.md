# ARM Assembly Lab
This repo is my lab to practice and learn ARM assembly from the book [Programming with 64-Bit ARM Assembly Language](https://learning.oreilly.com/library/view/programming-with-64-bit/9781484258811). The code stored in this repo is written by me, but all credit goes to 
the author of the mentioned book. As with all my [labs](https://github.com/trevorpiltch/Labs), this README  contains three sections, ABOUT, MOTIVATION, and BUILDING.

## MOTIVATION
My motivation behind learning assembly comes mainly from my work on the McGillRocket team, where I am in charge of the flight computer software. This system uses a lot of low-level C/C++ code that needs to be optimized. In order to better understand the code compilation, and write code more efficiently, I wanted to learn about how the code worked under the hood. Hence, I decided to learn assembly. 

## BUILDING
This code was tested on a Raspberry Pi 5. To build the assembly, run `./build.sh <FILE_NAME>`. Make sure that the `<FILE_NAME>` doesn't contain the assembly extension (`.s`). You will also need to add the directory to the filename to ensure that the script builds the right one. 


