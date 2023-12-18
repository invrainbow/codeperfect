import { Icon } from "@/components/Icon";
import { links } from "@/links";
import { Button } from "./components/Button";
import { Link } from "./components/Link";
import dom from "./components/dom";
import { ImageCarousel } from "./ImageCarousel";

export const Hero = () => (
  <dom.div cx="flex items-center px-4 md:px-8 md:py-4 max-w-screen-xl mx-auto flex-col md:flex-row gap-16">
    <dom.div cx="flex flex-col gap-6 md:gap-8 md:w-1/3 leading-relaxed px-4">
      <dom.div cx="font-black text-5xl md:text-6xl text-black tracking-tight leading-[1.175]">
        A fast, lightweight Go IDE
      </dom.div>
      <dom.div cx="flex md:text-center flex-col items-stretch self-stretch md:self-auto md:items-center md:flex-row gap-2 md:gap-4 md:justify-center">
        <Button href={links.download}>
          <Icon size={18} icon="Download" />
          Download for Mac
        </Button>
        <Button variant="secondary" href={links.docs}>
          View Docs
          <Icon
            size={18}
            icon="ChevronRight"
            cx="relative group-hover:translate-x-1 transition"
          />
        </Button>
      </dom.div>
      <dom.div cx="text-slate-500 text-sm font-medium leading-normal md:leading-snug inline-block">
        <b>Note:</b> CodePerfect is no longer actively developed, but is now{" "}
        <Link href={links.github}>open source</Link> and free.
      </dom.div>
    </dom.div>
    <dom.div cx="lg:flex-1">
      <ImageCarousel />
    </dom.div>
  </dom.div>
);
