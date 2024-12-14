 ## Building
 Clone the Alexandria repository.
```bash
$ git clone https://github.com/PGG106/Alexandria
```
Download the latest neural network for Alexandria from this [repository](https://github.com/PGG106/Alexandria-networks/releases) and place it in the root of the `Alexandria` folder.
```bash
$ cd alexandria
$ make 
$ ./Alexandria
```
 ## How to use the engine

The Universal Chess Interface (UCI) is a standard protocol used to communicate with
a chess engine, and is the recommended way to do so for typical graphical user interfaces
(GUI) or chess tools. Alexandria implements the majority of its options as described
in [the UCI protocol](https://www.shredderchess.com/download/div/uci.zip).

## Acknowledgements
This project would not have been possible without the following people
* BluefeverSoftware for his Vice chess engine from which i learnt the basic structure and functionality of a chess engine
* CodeMonkeyKing for his bbc chess engine from which i learnt how bitboards work and several refined search techniques
* The whole Stockfish Discord server and Disservin in particular for the sharing of code and the availability in answering questions
* Andrew Grant for the [OpenBench](https://github.com/AndyGrant/OpenBench) platform .
* Morgan Houppin, author of [Stash](https://github.com/mhouppin/stash-bot) for being a real G
* Older Alexandria's nets were trained with [Cudad](https://github.com/Luecx/CudAD), a big thanks to the authors of Cudad (Luecx and Jay Honnold), extra thanks to Luecx for initiating me in the way of NNUE.
* Current Alexandria's nets are trained with [Bullet](https://github.com/jw1912/bullet) the Official™️ SWE™️ Trainer™️, a big thanks to JW for being a Cuda god.