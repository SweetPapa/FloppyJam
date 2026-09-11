export const COLORS = [
  "#a6eed5",
  "#ffbe9d",
  "#c5b4f4",
  "#f5dda0",
  "#9fd8f4",
  "#efaec8",
];
export const DISTRICTS = [
  {
    name: "Harbor Square",
    short: "Harbor",
    x: 0,
    z: 0,
    color: COLORS[0],
    anchor: "Odette’s Bakery",
    npc: "Odette",
    parcel: "a sack of flour",
    puzzle: "phrase",
  },
  {
    name: "Cannery Row",
    short: "Docks",
    x: 64,
    z: 6,
    color: COLORS[1],
    anchor: "Bo’s Dock Office",
    npc: "Bo",
    parcel: "the dock manifest",
    puzzle: "stack",
  },
  {
    name: "The Undercroft",
    short: "Tunnels",
    x: 30,
    z: -62,
    color: COLORS[2],
    anchor: "Pearl’s Workshop",
    npc: "Pearl",
    parcel: "a tin of lamp oil",
    puzzle: "drill",
  },
  {
    name: "Hilltop Gardens",
    short: "Gardens",
    x: -45,
    z: -58,
    color: COLORS[3],
    anchor: "The Observatory",
    npc: "Idris",
    parcel: "a box of telescope parts",
    puzzle: "light",
  },
  {
    name: "The Point",
    short: "Lighthouse",
    x: -68,
    z: 17,
    color: COLORS[4],
    anchor: "The Lighthouse",
    npc: "Ada",
    parcel: "a parcel marked Return to sender",
    puzzle: "signal",
  },
];
export const WEATHER = [
  "Clear & still",
  "A little drizzle",
  "Rain on the rooftops",
  "The first snow",
  "Stars returning",
];
export const CLUES = [
  {
    id: "left-hand",
    holder: "Odette",
    clear: "Odette",
    title: "A left-handed lantern",
    text: "Odette noticed that every chalk lantern smudges to the left. The Lamplighter draws with their left hand.",
    secret:
      "Odette is writing a surprise cookbook of Brightwater family recipes.",
  },
  {
    id: "brass",
    holder: "Bo",
    clear: "Bo",
    title: "Bound for the boatyard",
    text: "The missing crates held brass fittings addressed to the boatyard. They were borrowed, not lost.",
    secret: "Bo has been taking singing lessons for Lantern Night.",
  },
  {
    id: "wrench",
    holder: "Pearl",
    clear: "Pearl",
    title: "A trail below town",
    text: "Heavy loads moved through the tunnels at night. Pearl found a wrench stamped with the boatyard mark.",
    secret: "Pearl is building a model train of the whole town.",
  },
  {
    id: "testing",
    holder: "Idris",
    clear: "Idris",
    title: "Three in the morning",
    text: "Idris saw light flickering in the lighthouse lamp room at 3 a.m. Someone was testing the lamp.",
    secret:
      "Idris is saving telescope parts for a surprise children’s star party.",
  },
  {
    id: "ada",
    holder: "Ada",
    clear: "Ada",
    title: "Kindness, returned",
    text: "Ada is the Lamplighter. She borrowed parts to rebuild the lighthouse lens for Gus, and repaired things as thanks.",
    secret:
      "Ada wanted to surprise her grandfather. The town would love to help.",
  },
];
export const CAST = {
  Winnie: {
    full: "Winnie Tallow",
    role: "Mayor",
    voice: "Cheerful and a little overcommitted",
    color: COLORS[5],
    samples: [
      "Oh, good, a courier!",
      "I have a list for my lists.",
      "There is always room at the table.",
    ],
    public:
      "Lantern Night is coming. The lens is cracked. Someone returns borrowed things repaired beside chalk lanterns.",
    warm: "I have quietly paid for the festival for years. A town needs things to look forward to.",
    hello:
      "Welcome, Kit. Someone is borrowing things, fixing them, and leaving chalk lanterns. Could you find our Lamplighter? Your case notebook is ready.",
  },
  Ferris: {
    full: "Ferris",
    role: "Night Owl Radio",
    voice: "Warm, unhurried radio host",
    color: COLORS[3],
    samples: [
      "Easy does it, night owl.",
      "Your route is waiting.",
      "No delivery is worth a hurry.",
    ],
    public:
      "You are the new night courier. I provide routes, weather and journal recaps.",
    warm: "You belong here already.",
    hello:
      "Evening, Kit. Follow the warm lantern to your delivery. No rush. Brightwater will wait for you.",
  },
  Odette: {
    full: "Odette Marchetti",
    role: "Baker",
    voice: "Chatty, generous, kitchen-table warmth",
    color: COLORS[0],
    samples: [
      "Come in out of the weather.",
      "A little flour fixes most things.",
      "There is a bun with your name on it.",
    ],
    public:
      "The rain scrambled my chalk menu. Please help restore the phrases.",
    warm: "I keep the town’s recipes safe.",
    hello:
      "Flour! You are a lovely sight. The rain has washed my menu clean. Shall we put the words back together?",
  },
  Bo: {
    full: "Bo Kalani",
    role: "Dockmaster",
    voice: "Few words, gentle and dry",
    color: COLORS[1],
    samples: ["Steady now.", "Good work. Tea?", "The tide can wait."],
    public: "The crates are tangled. Match four colors to clear the dock lane.",
    warm: "I have been practicing something for the festival.",
    hello:
      "Evening. Manifest received. Help me untangle these crates? Four of one color will clear a space.",
  },
  Pearl: {
    full: "Pearl Okafor",
    role: "Tunnel engineer",
    voice: "Precise, funny, proud of her tunnels",
    color: COLORS[2],
    samples: [
      "A good tunnel has excellent manners.",
      "Measure twice. Tea once.",
      "Mind the very small railway.",
    ],
    public:
      "A crate fell into a block-filled shaft. Drill connected colors to reach it.",
    warm: "I enjoy making small things work exactly right.",
    hello:
      "Lamp oil, precisely on time. A crate is stuck below my workshop. Let us clear a gentle path down.",
  },
  Idris: {
    full: "Prof. Idris Haddad",
    role: "Astronomer",
    voice: "Delighted, wandering but clear",
    color: COLORS[3],
    samples: [
      "Oh! Look at that.",
      "The sky is quite a generous neighbor.",
      "Where did I put my scarf?",
    ],
    public:
      "The telescope beams need routing through mirrors, splitters and colored lenses.",
    warm: "I hope the children get a clear sky.",
    hello:
      "My telescope parts! Wonderful. The eyepieces need light. Turn the mirrors and let us see where it goes.",
  },
  Gus: {
    full: "Gus Fenwick",
    role: "Retired lighthouse keeper",
    voice: "Gruff, tender underneath",
    color: COLORS[4],
    samples: [
      "Used to shine right across the bay.",
      "Well. That is something.",
      "Mind the lens, now.",
    ],
    public:
      "The lighthouse lens cracked. I think it is beyond fixing. Ada is my granddaughter and a boatyard mechanic.",
    warm: "I miss turning the crank on Lantern Night.",
    hello:
      "The old lens is cracked, Kit. I miss its light. Ada still comes up here with her tool belt. Good company, that girl.",
  },
  Ada: {
    full: "Ada Fenwick",
    role: "Boatyard mechanic",
    voice: "Shy, capable, speaks with care",
    color: COLORS[4],
    samples: [
      "I think I can make that work.",
      "Could you hold this? Thank you.",
      "It is easier with another pair of hands.",
    ],
    public:
      "I am a left-handed boatyard mechanic and Gus’s granddaughter. Gus taught me a signal-word game.",
    warm: "I like repairing things. It feels like saying thank you.",
    hello:
      "A returned parcel… for me? Could you help with Gus’s signal words? Nine beacons on each board. We can take our time.",
  },
  Rosie: {
    full: "Rosie",
    role: "Chalk Mural Club",
    voice: "Enthusiastic, kindly bossy",
    color: COLORS[5],
    samples: [
      "More chalk, please!",
      "That wall needs a boat.",
      "You can be in the club.",
    ],
    public:
      "Collect chalk around town. Each stick adds to our murals and changes your lantern trail.",
    warm: "You are officially in the club.",
    hello:
      "Welcome to the Chalk Mural Club! Find the glowing chalk sticks by the paths. Bring them to me and we will give these walls some color.",
  },
};
export const REPLIES = [
  "I’m glad I could help.",
  "Tell me about your work.",
  "What about the chalk lanterns?",
];
export const PHRASES = [
  [
    "BUTTER BUNS",
    "APPLE TART",
    "LEMON CAKE",
    "WARM BREAD",
    "PEACH PIE",
    "SUGAR COOKIES",
    "CINNAMON ROLLS",
    "CHERRY SCONES",
    "HONEY TOAST",
    "ALMOND BISCUITS",
    "COCOA MUFFINS",
    "GINGER SNAPS",
    "VANILLA CUSTARD",
    "BERRY CRUMBLE",
    "ORANGE MARMALADE",
    "PLUM PUDDING",
    "MAPLE WAFFLES",
    "OAT COOKIES",
    "CUSTARD TART",
    "RAISIN BREAD",
  ],
  [
    "A LITTLE KINDNESS",
    "FRESH FROM HOME",
    "FOLLOW THE LIGHT",
    "TEA FOR TWO",
    "WELCOME HOME COURIER",
    "BREAD AND BUTTER",
    "PASS THE SUGAR",
    "WARM YOUR HANDS",
    "MIND THE CRUMBS",
    "SHARE THE TABLE",
    "UNDER THE STARS",
    "LIGHT THE LANTERNS",
    "DOWN THE HILL",
    "ACROSS THE HARBOR",
    "THE KETTLE SINGS",
    "RAIN THEN RAINBOWS",
    "MAKE SOMETHING LOVELY",
    "ONE MORE BISCUIT",
    "A HELPING HAND",
    "HOME BEFORE DAWN",
  ],
  [
    "WE KNEAD A LITTLE KINDNESS",
    "THERE IS NO CRUMB PLACE",
    "YOU BAKE THE WORLD BRIGHTER",
    "A LITTLE FLOUR GOES FAR",
    "GOOD THINGS COME IN BATCHES",
    "HOME IS WHERE BREAD RISES",
    "EVERY LOAF HAS SILVER LININGS",
    "LET THE GOOD TIMES ROLL",
    "LIFE IS WHAT YOU BAKE",
    "THE YEAST WE CAN DO",
    "OUR TOWN TAKES THE CAKE",
    "A BUN IN EVERY WINDOW",
    "SOME BUNNY SAVED YOU ONE",
    "THE BEST IS YET DOUGH",
    "KEEP CALM AND CARRY BUNS",
    "KINDNESS IS OUR SECRET INGREDIENT",
    "WE RISE BY HELPING OTHERS",
    "THE EARLY BIRD GETS CRUMBS",
    "HERE TODAY AND SCONE TOMORROW",
    "YOU ARE MY BUTTER HALF",
  ],
];
export const SIGNAL_BANKS = [
  {
    theme: "Sea",
    groups: [
      { clue: "VESSEL", words: ["BOAT", "SHIP", "CANOE"] },
      { clue: "SHORELINE", words: ["BEACH", "SAND", "SHELL"] },
      { clue: "SAILING", words: ["MAST", "SAIL", "ANCHOR"] },
    ],
    fog: [
      "BREAD",
      "CLOUD",
      "SPOON",
      "GARDEN",
      "SCARF",
      "TRAIN",
      "CAKE",
      "PENCIL",
      "SNOW",
      "WINDOW",
      "APPLE",
      "CHAIR",
      "CLOCK",
      "BELL",
      "FLOWER",
    ],
    horn: "WHISTLE",
  },
  {
    theme: "Kitchen",
    groups: [
      { clue: "BAKING", words: ["FLOUR", "YEAST", "DOUGH"] },
      { clue: "CUTLERY", words: ["FORK", "KNIFE", "SPOON"] },
      { clue: "DESSERT", words: ["CAKE", "PIE", "COOKIE"] },
    ],
    fog: [
      "BOAT",
      "MOON",
      "RAIL",
      "LENS",
      "CAP",
      "WRENCH",
      "ROPE",
      "STAR",
      "SNOW",
      "CART",
      "CHALK",
      "TOWER",
      "FLAG",
      "BENCH",
      "WHEEL",
    ],
    horn: "WHISTLE",
  },
  {
    theme: "Sky",
    groups: [
      { clue: "CELESTIAL", words: ["STAR", "MOON", "SUN"] },
      { clue: "WEATHER", words: ["RAIN", "SNOW", "CLOUD"] },
      { clue: "OPTICS", words: ["LENS", "PRISM", "MIRROR"] },
    ],
    fog: [
      "BREAD",
      "BOAT",
      "CRATE",
      "TRAIN",
      "SPOON",
      "FLOWER",
      "CAP",
      "APPLE",
      "ROPE",
      "CHAIR",
      "DOUGH",
      "CART",
      "FLOUR",
      "BELL",
      "SAIL",
    ],
    horn: "WHISTLE",
  },
];
export function rng(seed) {
  let a = seed >>> 0;
  return () => {
    a += 0x6d2b79f5;
    let t = a;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}
export function shuffle(items, random) {
  const a = [...items];
  for (let i = a.length - 1; i > 0; i--) {
    const j = Math.floor(random() * (i + 1));
    [a[i], a[j]] = [a[j], a[i]];
  }
  return a;
}
export function decorations(seed) {
  const random = rng(seed);
  return DISTRICTS.flatMap((d, k) =>
    Array.from({ length: 18 }, (_, i) => {
      const angle = (i / 18) * Math.PI * 2;
      const r = 22 + random() * 9;
      return {
        district: k,
        x:
          d.x +
          Math.cos(angle) * r +
          (Math.sin(angle) > 0.5 && Math.abs(Math.cos(angle) * r) < 13
            ? Math.cos(angle) < 0
              ? -13
              : 13
            : 0),
        z: d.z + Math.sin(angle) * r,
        w: 3 + random() * 3,
        h: 3 + random() * 8,
        roof: random() > 0.5,
      };
    }),
  );
}
export function unlockedKnowledge(name, state) {
  const c = CAST[name];
  const facts = [c.public];
  if ((state.warmth[name] || 0) > 0) facts.push(c.warm);
  const clue = CLUES.find((c) => c.holder === name);
  if (clue && state.facts.includes(clue.id)) facts.push(clue.secret, clue.text);
  return facts;
}
export function fallbackDialogue(name, state, beat = "hello") {
  const clue = CLUES.find(
    (c) => c.holder === name && state.facts.includes(c.id),
  );
  return {
    say:
      beat === "case"
        ? clue
          ? `${clue.secret} ${clue.text}`
          : "I have noticed those little chalk lanterns too. Your notebook will keep the facts straight."
        : beat === "work"
          ? CAST[name].public
          : beat === "warm"
            ? CAST[name].warm
            : CAST[name].hello,
    mood: "warm",
    reveal_clue_ids: clue ? [clue.id] : [],
    suggested_replies: REPLIES,
    end_conversation: false,
  };
}
