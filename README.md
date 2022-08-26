# Remyxer
Real time audio multitrack mixer/recorder/player.

Remyxer is dedicated to musician work, and use mulitple USB audio input/output device.

It is supposed to act like a recording studio but more oriented to musician work.

One input/ouput per musician to capture it own sound and play it in real time to the headphone.

Remyxer add a wav file during real time sound directly copied from the input (microphne) to the output (headphones).

The musician can play along with the wav sound track will hearing it own sound from the microphone.

Remyxer can handle several USB interfaces, one per musician.

Every musician will hear its own sound mixed with the wav sound track but it can also mix the others musician sound like a audio multi track mixer.

Remyxer can record all musician tracks independently and produce a wav file for each musician.

Once recorded, Remyxer can playback all the tracks, or record one or more will playing the others, or whatever you need.

Remyxer can upload the wav files to an FTP server to share it with the other band members working on the same partition, then musicians can work even if a musician in not present or if they are not together, they will still have the recorded sounds.

Remyxer is compiled with Qt for the Gui part, uses portaudio to drive real time multitrack audio input output, and libsndfile to read and write wav files.

Audio sampling rate can be 44,1kHz or 48kHz. Wav files will be converted by Remyxer if they don't match the device audio sampling rate, but for the moment the conversion algoryhthm is not accurate to give a perfect sound. The best is to first convert with well known software like Audacity.
