type point {x real, y real}

type address {street string, building int}

var p point;

var a address;

fn cannot_semidiff() point
{
	return (minus p a);
}
