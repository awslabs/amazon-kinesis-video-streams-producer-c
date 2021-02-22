Release 1.1.1 of the Amazon Kinesis Video C Producer SDK

Platforms tested on:

Linux
MacOS
Windows
x64
ARMv5

Release tagged at: f2a97fe6eaf78cbffd46ccfa5994bee2bebf99bf

Bug fixes: 
- Use appropriate Frame order mode by default based on number of tracks
- Fixed a bug to ensure API cache is not deleted when `resetStream()` API is invoked

PIC commit: df42dddc1d421d1e6bc47d5ebf7cd085446cbb69