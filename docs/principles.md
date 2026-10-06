# Principles

## Content over chrome

No cards, no panels, no boxed backgrounds for ordinary content. Tiles are
the one exception — a tile's whole fill *is* information, not a
container around information (see [tiles.md](tiles.md)). Everything else
sits directly on the page's ground colour, grouped by whitespace and thin
hairlines (1–2px borders), never by a background shape. A row is the
control; nothing needs a box around it to be clickable or readable.

Why: boxes compete with the data they hold for visual weight. Remove
them and the eye reads the numbers and words first.

## Motion as guidance

A transition is never decorative. Each one answers a question for the
user without words:

- *Where am I now?* — a place change.
- *Same place, different facet?* — a pivot change.
- *Am I just looking sideways?* — a panorama scroll.
- *Is this live, or static?* — staggered build-in, drawing lines, a live
  pulse.
- *What did my action just do?* — a reordering animation (FLIP) so the
  user sees cause and effect instead of a sudden jump.

Every duration and easing curve is a token (see [motion.md](motion.md)),
and the whole system turns off under reduced-motion settings — turning
it off must never hide information, only the transition between states.

## Information-dense live tiles

A tile is sized by the importance of what it shows, not by a fixed grid
cell everything must fit into. A question that needs an answer is bigger
than a finished item. A tile's fill colour carries its status at a
glance; small marks (a dot, a number, a progress bar) carry the colour
at full saturation, large fills stay muted so the page doesn't shout.
See [tiles.md](tiles.md).

## Panorama and pivot

Two distinct navigation shapes, used for two distinct kinds of
structure:

- A **panorama** is one place that is wider than the viewport. The user
  moves through it sideways — by pointer drag, by mouse wheel on
  desktop, by swipe on touch. It is for a single screen with more
  content than fits, grouped into named sections.
- A **pivot** is one place's ordered, named facets (think tabs, but the
  active one is large and the others are visible, dimmed, in a fixed
  order). It is for switching between views of the *same* underlying
  thing, not for unrelated destinations.

Both exist at every level a product needs: a top-level pivot picks the
place; a panorama or a nested pivot organises what's inside that place.
See [navigation.md](navigation.md) and [layout.md](layout.md).

## Refined precision

The aesthetic is calm and exact, not playful: hairlines instead of
shadows, tabular figures for numbers that line up in a column, muted
large fills with vivid small accents, a restrained three-level type
hierarchy per screen. Precision is also functional — every status fill
is checked for AA contrast, every focus ring is visible on every
background it can land on, every motion token has a reduced-motion
value. Nothing here is decoration for its own sake.
