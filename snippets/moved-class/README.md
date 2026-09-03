# Deriving All Special Member Functions from Two

A class only needs to actually *implement* a default constructor, a copy
constructor, and a move-assignment operator; the move constructor and
copy-assignment operator can both be written mechanically in terms of
those three (move-construct by default-constructing then move-assigning;
copy-assign by copy-constructing a temporary then move-assigning it --
the copy-and-swap-adjacent idiom). Not a general recommendation to write
every class this way, but a useful way to see how the five special member
functions relate to each other.
