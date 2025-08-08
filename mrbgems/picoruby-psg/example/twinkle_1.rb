tracks = [
  '@0 T120 S0 M800 L4 O5 f  f >c   c |  d    d   c2    |<b-  b-  a  a | g  g f2|>c  c <b- b-| a  a  g2     |>c   c  <b- b- | a  a g2        |f  f> c   c |  d    d   c2    | <b- b-  a  a | g  g f2',
  '@0 T120 S0      L4 O4 a >c  f   f |  f    f   g   f | e   c   f  c |<b- b-a2|>a  a  g  g | f  f  e2     | g   f   d  e  | c  f e8d8c8<b-8|a >c  f   f |  f    f   g   f |  e  c   f  c |<b- b-a2',
  '@1 T120 S0      L8 O3 f>ca4<a>e>c4|<<b->f>d4<<a>f>c4|<<g>eb-4<f>ca4| c4<c4f2| f>ca4<cg>e4|<f>ca4<cg>e<b-| a>f>c4<<g>eb-4|<f>ca4c4 <c4    |f>ca4<a>e>c4|<<b->f>d4<<a>f>c4|<<g>eb-4<f>ca4| c4<c4f2'
]

driver = PSG::Driver.new(:pwm, left: 10, right: 11)

driver.play_mml(tracks)


