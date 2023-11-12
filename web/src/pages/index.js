import { A, Icon } from "@/components";
import { FEATURE_LIST, LINKS } from "@/constants";
import { IconChevronRight, IconDownload } from "@tabler/icons-react";

const BAD_FEATURES = [
  "Electron",
  "JavaScript",
  "language servers",
  "frameworks",
  "pointless abstractions",
];

export default function Home() {
  return (
    <div className="bg-neutral-50 mx-auto w-full">
      <div className="bg-white flex flex-col gap-12 max-w-full leading-relaxed px-8 py-12 md:pt-20 md:pb-24">
        <div>
          <div className="md:text-center font-bold text-5xl text-black tracking-tight leading-[1.1] md:leading-none mb-6">
            A fast, lightweight Go IDE
          </div>
          <div className="max-w-[550px] mx-auto leading-relaxed">
            <p>
              CodePerfect is a fast Go IDE, built from scratch in C/C++/OpenGL
              like a video game. It starts instantly, runs at 144 FPS, has
              near-zero latency, and comes with native, full-featured code
              intelligence and integrated debugging with Delve. See more
              features <A href="/features">here</A>.
            </p>
            <p>
              It's no longer actively developed, but is now{" "}
              <A href={LINKS.github}>open source</A> and free.
            </p>
          </div>
        </div>
        <div className="md:text-center flex flex-col md:flex-row gap-4 md:justify-center">
          <A
            href={LINKS.download}
            className="btn btn1 justify-center flex md:inline-flex text-center"
          >
            <Icon size={18} className="mr-1" icon={IconDownload} />
            Download for Mac
          </A>
          <A
            href={LINKS.docs}
            className="btn btn2 justify-center flex md:inline-flex text-center"
          >
            View Docs
            <Icon size={18} className="ml-1" icon={IconChevronRight} />
          </A>
        </div>
      </div>

      <div className="bg-neutral-900 border-gray-100 p-8 py-12 md:pb-24 relative z-10 flex items-center justify-center">
        <div className="max-w-screen-xl flex flex-col lg:flex-row items-center gap-8 md:gap-16">
          <div className="lg:w-1/3 md:mx-0">
            <div className="text-[160%] md:text-[200%] font-extrabold text-black leading-tight">
              {BAD_FEATURES.map((name) => (
                <div key={name} className="text-white md:whitespace-nowrap">
                  No {name}.
                </div>
              ))}
            </div>
            <div className="text-lg leading-normal mt-6 md:mt-8 text-neutral-400">
              <p>
                We threw out the modern software stack and rebuilt the IDE in
                blazing fast C/C++.
              </p>
              <p>
                Built like a video game. Instant startup. 144 FPS. A zero
                latency experience. An indexer that gobbles through large
                codebases. CodePerfect does the right thing and gets out of your
                way.
              </p>
            </div>
          </div>
          <div className="md:flex-1">
            <A target="_blank" href="/download.png">
              <img
                className="max-w-full shadow-lg rounded-lg overflow-hidden"
                alt="screenshot"
                src="/download.png"
              />
            </A>
          </div>
        </div>
      </div>

      <div className="batteries-included z-10 px-6 md:px-12 py-12 md:py-20">
        <div className="max-w-screen-lg mx-auto flex flex-col md:flex-row gap-8 items-center">
          <div className="md:w-1/3">
            <h1 className="title text-3xl md:text-4xl">
              <div>Batteries included,</div>
              <div>zero configuration.</div>
            </h1>
            <div className="max-w-screen-sm mx-auto mt-4 mb-6 text-lg">
              <p>
                Get the best of both worlds: the speed of Vim, the power of an
                IDE.
              </p>
            </div>
            <A
              href="/features"
              className="btn btn2 btn-lg btn-no-hover justify-center flex md:inline-flex text-center group"
            >
              View Features
              <Icon
                size={18}
                className="ml-1.5 relative top-[1px] group-hover:translate-x-1 transition"
                icon={IconChevronRight}
              />
            </A>
          </div>

          <div className="hidden md:block flex-1">
            <div className="flex flex-wrap justify-center gap-x-8 gap-y-3 text-[110%] rounded p-6 font-mono">
              {FEATURE_LIST.map((it) => (
                <div
                  key={it.name}
                  className="flex-shrink leading-none text-center text-neutral-300 hover:text-neutral-400 transition-colors"
                >
                  {it.name}
                </div>
              ))}
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
