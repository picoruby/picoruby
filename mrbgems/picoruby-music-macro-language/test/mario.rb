#!/usr/bin/env ruby

load "../mrblib/mml.rb"

tracks = [
  'T80 S0 M3000 O5 | L8 E  E  R E  R C  E  R |  G4 R4 < G4 R4',
  'T80 S0       O4 | L8 F# F# R F# R F# F# R |  B4 R4   R2   ',
  'T80 S0       O3 | L8 D  D  R D  R D  D  R |> G4 R4 < G4 R4',
]

MML.compile_multi(tracks, exception: false) do |delta, tr, command, *args|
  puts "tr: #{tr}, delta: #{delta}, command: #{command}, args: #{args.inspect}"
end
