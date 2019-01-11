# Antenna choices

You'll need two antennas: one for the TX and another for the RX. The most common choice is to have a directional one _(moxon, folded dipole)_ on the TX and an omnidirectional one _(dipole, vee)_ on the RX. Be sure to match them to the frequency you chose.

+ Moxon: [Prodrone][prodrone_shop], [Actuna][actuna_shop]
+ Folded Dipole: [FrSky Super8][horusrc_super8], [TBS Diamond][tbs_diamond]
+ Dipole: [Prodrone][prodrone_shop], [Actuna][actuna_shop], [FrSky T][horusrc_t], [TBS Immortal T][tbs_t]

These kinds of antennas are also really easy to DIY if you have some copper wire handy (and possibly a 3D printer). Here are some examples: [Moxon][pawel_moxon], [Dipole][pawel_dipole].

# Feed line length

The feed line is the length of cable that goes from your TX to the antenna. For optimal performance, it should be made of cabling with the correct impedance and have a determined length.

Antenna theory dictates strict rules, but in real world applications the environment you'll be using Raven in will present way more complications than what a "wrong" feed line length will ever cause.

Use this as a _vague_ rule of thumb: use some decent coaxial cable and try to keep its length close to half (or multiples) of your chosen frequency's wavelength. There are a lot of [dedicated calculators](http://www.procato.com/calculator-wavelength-frequency/) online.

|     | 433MHz | 868MHz | 915MHz |
|-----|--------|--------|--------|
| 1/2 | 346 mm | 173 mm | 164 mm |
| 1/4 | 173 mm | 86 mm  | 82 mm  |
| 1/8 | 86 mm  | 43 mm  | 41 mm  |



[prodrone_shop]: https://shop.prodrone.pl/
[actuna_shop]: https://www.actuna.com/en/52-rc-antennas
[horusrc_super8]: https://www.horusrc.com/en/frsky-900mhz-super-8-antenna-for-r9m-r9m-lite-module.html
[tbs_diamond]: https://team-blacksheep.com/products/prod:diamond_antenna
[horusrc_t]: https://www.horusrc.com/en/frsky-900mhz-ipex1-dipole-t-antenna-for-r9-slim-r9-slim-receiver.html
[tbs_t]: https://www.team-blacksheep.com/products/prod:xf_immortal_t
[pawel_moxon]: https://www.youtube.com/watch?v=vV3SOVvokvA
[pawel_dipole]: https://www.youtube.com/watch?v=eQEmFlMW50o