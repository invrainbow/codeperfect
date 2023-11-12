import { A } from "@/components";
import { FEATURE_LIST, LINKS } from "@/constants";

export default function Features() {
  const onScroll = (name) => {
    const elem = document.querySelector(`[data-feature-name="${name}"]`);
    if (elem) {
      elem.scrollIntoView({
        behavior: "smooth",
      });
    }
  };

  return (
    <div className="max-w-screen-lg flex mx-auto mt-8 border-b border-neutral-100 md:border-0 md:my-16 gap-12 features">
      <div className="w-[175px] hidden md:block">
        <div className="sticky top-8">
          {FEATURE_LIST.map((it) => (
            <div key={it.name} className="mb-3 last:mb-0 leading-none">
              <button
                onClick={() => onScroll(it.name)}
                className="p-0 leading-none text-[95%] text-neutral-600 hover:text-neutral-900 text-left"
                key={it.name}
              >
                {it.name}
              </button>
            </div>
          ))}
        </div>
      </div>
      <div className="flex-1">
        <div className="px-5 md:px-0">
          <div className="title text-3xl md:text-5xl mb-4">Features</div>
          <div className="mb-8">
            This is a brief overview of the features inside CodePerfect. For a
            more complete and in-depth list, see the full{" "}
            <A href={LINKS.docs}>docs</A>.
          </div>
        </div>
        <div className="">
          {FEATURE_LIST.map((it) => (
            <div
              data-feature-name={it.name}
              key={it.name}
              className="bg-white md:rounded-lg md:shadow-lg md:mt-8 p-6 border-b last:border-0 border-neutral-100 md:border-0 md:p-8 md:first:mt-0"
            >
              <div className="mb-4 font-bold text-lg leading-none">
                {it.name}
              </div>
              <div className="leading-relaxed">{it.body}</div>
            </div>
          ))}
        </div>
      </div>
    </div>
  );
}
