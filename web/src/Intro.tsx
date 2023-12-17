import { links } from "@/links";
import { ImageCarousel } from "./ImageCarousel";
import { A } from "@/components/A";

const BAD_FEATURES = [
  "Electron",
  "JavaScript",
  "language servers",
  "frameworks",
];

export const Intro = () => (
  <div className="px-4 md:px-8 md:py-4 max-w-screen-xl mx-auto flex flex-col md:flex-row items-center gap-16">
    <div className="md:w-1/3 md:mx-0 flex flex-col gap-6">
      <div className="flex flex-col leading-none gap-2 text-[160%] md:text-[200%] font-extrabold text-black">
        {BAD_FEATURES.map((name) => (
          <div key={name} className="text-black md:whitespace-nowrap">
            No {name}.
          </div>
        ))}
      </div>
      <div className="leading-relaxed text-neutral-600">
        <p>
          CodePerfect is built from scratch in C/C++/OpenGL like a video game.
          Instant startup, 144 FPS, near-zero latency, native code intelligence.
        </p>
      </div>
      <div>
        <A className="btn btn1" href={links.docs}>
          See features
        </A>
      </div>
    </div>
    <div className="lg:flex-1">
      <ImageCarousel />
    </div>
  </div>
);
