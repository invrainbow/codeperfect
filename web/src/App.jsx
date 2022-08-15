import { AiFillApple } from "@react-icons/all-files/ai/AiFillApple";
import { AiFillWindows } from "@react-icons/all-files/ai/AiFillWindows";
import { AiOutlineCheck } from "@react-icons/all-files/ai/AiOutlineCheck";
import { AiOutlineClose } from "@react-icons/all-files/ai/AiOutlineClose";
import { BsChevronRight } from "@react-icons/all-files/bs/BsChevronRight";
import { BsCodeSlash } from "@react-icons/all-files/bs/BsCodeSlash";
import { FaLayerGroup } from "@react-icons/all-files/fa/FaLayerGroup";
import { FaLinux } from "@react-icons/all-files/fa/FaLinux";
import { FaRobot } from "@react-icons/all-files/fa/FaRobot";
import { GoPackage } from "@react-icons/all-files/go/GoPackage";
import { GrInfo } from "@react-icons/all-files/gr/GrInfo";
import { IoMdSearch } from "@react-icons/all-files/io/IoMdSearch";
import { HiOutlineDownload } from "@react-icons/all-files/hi/HiOutlineDownload";
import { ImMagicWand } from "@react-icons/all-files/im/ImMagicWand";
import { SiVim } from "@react-icons/all-files/si/SiVim";

import posthog from "posthog-js";
import cx from "classnames";
import React from "react";
import { Helmet } from "react-helmet";
import { useMediaQuery } from "react-responsive";
import {
  BrowserRouter,
  Link,
  Outlet,
  Navigate,
  Route,
  Routes,
  useLocation,
} from "react-router-dom";
import "./index.css";

posthog.init("phc_kIt8VSMD8I2ScNhnjWDU2NmrK9kLIL3cHWpkgCX3Blw", {
  api_host: "https://app.posthog.com",
});

const SUPPORT_EMAIL = "support@codeperfect95.com";
const CURRENT_BUILD = "22.08";

const LINKS = {
  docs: "https://docs.codeperfect95.com",
  gettingStarted: "https://docs.codeperfect95.com/getting-started",
  changelog: "https://docs.codeperfect95.com/changelog",
  buyPersonalMonthly: "https://buy.stripe.com/aEU5kx2aTaso4TK008",
  buyPersonalYearly: "https://buy.stripe.com/fZefZb2aTdEAbi8aEN",
  buyProfessionalMonthly: "https://buy.stripe.com/6oE28ldTB5843PG9AK",
  buyProfessionalYearly: "https://buy.stripe.com/28o8wJ3eXfMI4TK5kv",
};

const CDN_PATH = "https://d2mje1xp79ofdv.cloudfront.net";

function asset(path) {
  const dev = process.env.NODE_ENV === "development";
  return dev ? `/public/${path}` : `${CDN_PATH}${path}`;
}

function A({ link, children, href, ...props }) {
  if (link) {
    return (
      <Link to={href} {...props}>
        {children}
      </Link>
    );
  }
  return (
    <a href={href} {...props}>
      {children}
    </a>
  );
}

function WallOfText({ width, children, className, ...props }) {
  // for tailwindcss shake:
  // md:max-w-0 md:max-w-none md:max-w-xs md:max-w-sm md:max-w-md md:max-w-lg
  // md:max-w-xl md:max-w-2xl md:max-w-3xl md:max-w-4xl md:max-w-5xl md:max-w-6xl
  // md:max-w-7xl md:max-w-full md:max-w-min md:max-w-max md:max-w-fit md:max-w-prose
  // md:max-w-screen-sm md:max-w-screen-md md:max-w-screen-lg md:max-w-screen-xl
  // md:max-w-screen-2xl

  return (
    <div
      className={cx(
        "wall-of-text p-4 py-10 md:py-28 leading-normal md:mx-auto",
        `md:max-w-${width || "2xl"}`,
        className
      )}
      {...props}
    >
      {children}
    </div>
  );
}

function H1({ className, children, ...props }) {
  return (
    <h2
      className={cx("mb-6 text-2xl font-bold text-gray-800", className)}
      {...props}
    >
      {children}
    </h2>
  );
}

function Title({ className, ...props }) {
  return <H1 className={cx("text-3xl", className)} {...props} />;
}

function Icon({ block, noshift, icon: IconComponent, ...props }) {
  return (
    <span className={block ? "block" : "inline-block"}>
      <IconComponent {...props} />
    </span>
  );
}

const FEATURES = [
  {
    name: "Code navigation",
    icon: BsCodeSlash,
    description: "Jump to definition and find all references.",
    videoId: "QRI09kTuyPQ",
  },
  {
    name: "Vim keybindings",
    description: "Full vim keybindings out of the box.",
    icon: SiVim,
    videoId: "nO_-RT8-iHM",
  },
  {
    name: "Automatic rename",
    description: "Rename any identifier along with all references to it.",
    videoId: "GPwc2vuK4sA",
    icon: ImMagicWand,
  },
  {
    name: "Postfix completions",
    description:
      "Generate code with macros that work intelligently on Go expressions.",
    icon: FaRobot,
    videoId: "qMAachk4TGI",
  },
  {
    name: "Native interface support",
    description: "Navigate and generate interfaces quickly and easily.",
    icon: FaLayerGroup,
    videoId: "CsxmGcahc4k",
  },
  {
    name: "Instant fuzzy search",
    icon: IoMdSearch,
    description: "Works on files, symbols, commands, and completions.",
    videoId: "9tl5rP97pg4",
  },
  {
    name: "Automatic import",
    description: "Pull in libraries you need without moving your cursor.",
    icon: GoPackage,
    videoId: "XrZTgAVXaQ0",
  },
  {
    name: "Parameter hints",
    icon: GrInfo,
    description: "View function signatures as you're calling them.",
    videoId: "TzJUEFedUIo",
  },
];

function YoutubeEmbed({ videoId, first }) {
  const autoplay = first ? 0 : 1;
  return (
    <div
      className="relative h-0 rounded-lg border-2 border-black overflow-hidden shadow-md"
      style={{ paddingBottom: "58.60%" }}
    >
      <iframe
        className="absolute t-0 l-0 w-full h-full"
        src={`https://www.youtube.com/embed/${videoId}?autoplay=${autoplay}&loop=1&playlist=${videoId}&showinfo=0&controls=1&modestbranding=1`}
        title="YouTube video player"
        frameborder="0"
        allow="accelerometer; autoplay; clipboard-write; encrypted-media; fullscreen; gyroscope; picture-in-picture"
        allowfullscreen
      />
    </div>
  );
}

function VideoPlayer() {
  const [selected, setSelected] = React.useState(FEATURES[0]);
  const [first, setFirst] = React.useState(true);
  const isbig = useMediaQuery({ query: "(min-width: 768px)" });

  return (
    <div
      className="md:grid w-full md:max-w-screen-xl mx-auto md:gap-x-6 text-sm"
      style={{ "grid-template-columns": "250px auto" }}
    >
      <div className="max-w-full md:max-w-xs">
        {FEATURES.map((it) => {
          const isSelected = selected && selected.name === it.name;
          return (
            <button
              className={cx(
                "text-left block w-full rounded-md relative mb-1 overflow-hidden",
                isSelected ? "opacity-100" : "opacity-70"
              )}
              style={{
                background: isSelected
                  ? "rgba(0, 0, 0, 0.2)"
                  : "rgba(0, 0, 0, 0.1)",
              }}
              onClick={() => {
                setFirst(false);
                setSelected(it);
              }}
            >
              <div className="p-3">
                <div className="flex flex-row items-center gap-x-2">
                  <div
                    className={cx(
                      "text-lg leading-none text-black",
                      isSelected ? "opacity-90" : "opacity-60"
                    )}
                  >
                    <Icon block icon={it.icon} />
                  </div>
                  <div
                    className={cx(
                      "font-semibold leading-none relative",
                      isSelected ? "text-black" : "text-gray-700"
                    )}
                    style={{ paddingTop: "-1px" }}
                  >
                    {it.name}
                  </div>
                </div>
                <div
                  className={cx(
                    "text-sm text-gray-700 overflow-hidden transition-all duration-100 ease-linear",
                    isSelected ? "mt-1.5 max-h-16" : "mt-0 max-h-0"
                  )}
                >
                  {it.description}
                </div>
              </div>
              {isSelected && selected.videoId && !isbig && (
                <YoutubeEmbed videoId={selected.videoId} first={first} />
              )}
            </button>
          );
        })}
      </div>
      {selected && selected.videoId && isbig && (
        <YoutubeEmbed videoId={selected.videoId} first={first} />
      )}
    </div>
  );
}

function Home() {
  return (
    <div className="my-8 md:my-20 w-full">
      <div className="max-w-full md:max-w-xl md:mx-auto text-lg md:text-lg leading-relaxed mb-16 md:mb-24 p-4">
        <div className="text-center font-bold text-3xl md:text-4xl mb-8 text-black">
          A High Performance IDE for Go
        </div>
        <p>
          Written in C++, CodePerfect indexes large codebases quickly and
          responds to every key in 16ms. Supports Windows, Mac and Linux.
        </p>
        <p>
          Built for Vim users who want more power, Jetbrains users who want more
          speed, and everyone in between.
        </p>

        <div className="mt-8 text-center flex gap-4 justify-center">
          <A
            link
            href="/download"
            className="button main-button justify-center flex md:inline-flex text-center"
          >
            <Icon className="mr-2" icon={HiOutlineDownload} />
            Download
          </A>
          <A
            href="https://docs.codeperfect95.com"
            className="button download-button justify-center flex md:inline-flex text-center"
          >
            View Docs
            <Icon className="ml-2" icon={BsChevronRight} />
          </A>
        </div>
      </div>

      <div className="px-4">
        <div className="mb-8 font-bold text-black text-3xl text-center">
          A full-featured IDE, as fast as Sublime Text.
        </div>
        <VideoPlayer />
      </div>
    </div>
  );
}

function PaymentDone() {
  return (
    <WallOfText>
      <Title>Thanks for the purchase!</Title>
      <p>
        Please check your email &mdash; we just sent your license key to you. If
        you don't see it, see if it's in your spam folder. If you still don't
        see it, please <a href={`mailto:${SUPPORT_EMAIL}`}>contact support</a>.
      </p>
    </WallOfText>
  );
}

function PortalDone() {
  return (
    <WallOfText>
      <p className="text-center">
        You just left the billing portal. You can now close this window.
      </p>
    </WallOfText>
  );
}

function BuyLicenseButton({ className, ...props }) {
  return (
    <A
      className={cx(
        className,
        "button download-button p-3 block text-center bg-white"
      )}
      {...props}
    />
  );
}

function BuyLicenseBox({ className, ...props }) {
  return (
    <div
      className={cx(
        className,
        "w-auto overflow-hidden border rounded-md shadow-sm"
      )}
      {...props}
    />
  );
}

const disableButtonProps = {
  onClick: (e) => {
    e.preventDefault();
    alert(
      "CodePerfect is currently in maintenance mode and is not available for download. Please check back later."
    );
  },
};

function BuyLicense() {
  return (
    <div className="max-w-screen-md px-4 mx-auto my-16 md:my-28">
      <Title>Buy CodePerfect</Title>
      <p>
        CodePerfect is free to evaluate for 7 days, after which you'll need a
        license to keep using it.
      </p>
      <div className="my-6 grid grid-cols-1 sm:grid-cols-2 gap-6">
        <BuyLicenseBox>
          <div className="p-5">
            <h1 className="font-bold text-gray-700 mb-2">Personal</h1>
            <PricingPoint label="Commercial use ok" />
            <PricingPoint label="All features available" />
            <PricingPoint not label="Company can't pay" />
            <PricingPoint not label="Purchase can't be expensed" />
          </div>

          <div className="p-5 border-t border-gray-100 bg-gray-50">
            <div className="grid grid-cols-2 gap-3">
              <div>
                <div className="flex items-center mb-2.5">
                  <div className="leading-none text-xl font-bold text-gray-700">
                    $5
                  </div>
                  <div className="leading-none text-xs ml-1">/month</div>
                </div>
                <BuyLicenseButton href={LINKS.buyPersonalMonthly}>
                  Buy Monthly
                </BuyLicenseButton>
              </div>
              <div>
                <div className="flex items-center mb-2.5">
                  <div className="leading-none text-xl font-bold text-gray-700">
                    $50
                  </div>
                  <div className="leading-none text-xs ml-1">/year</div>
                </div>
                <BuyLicenseButton href={LINKS.buyPersonalYearly}>
                  Buy Yearly
                </BuyLicenseButton>
              </div>
            </div>
          </div>
        </BuyLicenseBox>
        <BuyLicenseBox>
          <div className="p-5">
            <h1 className="font-bold text-gray-700 mb-2">Pro</h1>
            <PricingPoint label="Commercial use ok" />
            <PricingPoint label="All features available" />
            <PricingPoint label="Company can pay" />
            <PricingPoint label="Purchase can be expensed" />
          </div>

          <div className="p-5 border-t border-gray-100 bg-gray-50">
            <div className="grid grid-cols-2 gap-3">
              <div>
                <div className="flex items-center mb-2.5">
                  <div className="leading-none text-xl font-bold text-gray-700">
                    $10
                  </div>
                  <div className="leading-none text-xs ml-1">/month</div>
                </div>
                <BuyLicenseButton href={LINKS.buyProfessionalMonthly}>
                  Buy Monthly
                </BuyLicenseButton>
              </div>
              <div>
                <div className="flex items-center mb-2.5">
                  <div className="leading-none text-xl font-bold text-gray-700">
                    $100
                  </div>
                  <div className="leading-none text-xs ml-1">/year</div>
                </div>
                <BuyLicenseButton href={LINKS.buyProfessionalYearly}>
                  Buy Yearly
                </BuyLicenseButton>
              </div>
            </div>
          </div>
        </BuyLicenseBox>
      </div>
      <p>
        We'll send your license key to the email you provide during checkout.
      </p>
      <p>
        Licenses are per-user. If you want to buy licenses in bulk, or have any
        other custom requests, please{" "}
        <a href={`mailto:${SUPPORT_EMAIL}`}>contact support</a>.
      </p>
    </div>
  );
}

function Download() {
  const url = "/";

  const links = [
    { platform: "windows-x64", icon: AiFillWindows, label: "Windows", url },
    { platform: "mac-x64", icon: AiFillApple, label: "macOS — Intel", url },
    { platform: "mac-arm", icon: AiFillApple, label: "macOS — M1", url },
    { platform: "linux-x64", icon: FaLinux, label: "Linux", url },
  ];

  const supportingLinks = (
    <p>
      See also{" "}
      <A href="https://docs.codeperfect95.com/getting-started/">
        Getting Started
      </A>
      .
    </p>
  );

  return (
    <div className="max-w-screen-md px-4 mx-auto my-16 md:my-28">
      <Title>Download CodePerfect</Title>
      <p>
        CodePerfect is free to evaluate for 7 days, with all features available.
        After the trial period you'll need a{" "}
        <A link href="/buy">
          license
        </A>{" "}
        for continued use.
      </p>
      <div className="p-4 my-6 rounded bg-gray-100 border border-gray-400">
        <div className="font-bold mb-3">Build {CURRENT_BUILD}</div>
        <div className="flex flex-col gap-2">
          {links.map((it) => (
            <div>
              <A
                href={it.url}
                {...disableButtonProps}
                className="button download-button text-sm px-3 py-2"
                onClick={() =>
                  posthog.capture("download", { platform: it.platform })
                }
              >
                <Icon className="mr-1" icon={it.icon} />
                Download for {it.label}
              </A>
            </div>
          ))}
        </div>
      </div>
      {supportingLinks}
    </div>
  );
}

function Terms() {
  return (
    <WallOfText>
      <H1>Terms of Service</H1>
      <p>
        This website provides you with information about the IDE and a means for
        you to subscribe to our services, which allow you to use the IDE for as
        long as your subscription is active.
      </p>
      <p>
        The IDE is an application that lets you write Go applications. In
        exchange for paying a monthly rate, we provide you with a license to use
        it.
      </p>

      <div className="mt-16">
        <H1>Privacy Policy</H1>
        <p>
          When you sign up, we collect your name, email, and credit card
          information. We use this information to bill you and send you emails
          with updates about your payment status (for example, if your card
          fails).
        </p>
        <p>
          The IDE contacts the server to authenticate your license key and to
          install automatic updates. This exposes your IP address to us. We
          won't share it with anyone, unless ordered to by law.
        </p>
      </div>
    </WallOfText>
  );
}

function ScrollToTop() {
  const { pathname, hash } = useLocation();

  React.useEffect(() => {
    if (!hash) {
      window.scrollTo(0, 0);
    }
  }, [pathname, hash]);

  return null;
}

function PricingPoint({ label, not }) {
  return (
    <div
      className={cx(
        "flex items-start space-x-1 leading-6 mb-1 last:mb-0",
        not && "text-red-600"
      )}
    >
      <Icon
        className="relative top-1 transform scale-90 origin-center"
        icon={not ? AiOutlineClose : AiOutlineCheck}
      />
      <span>{label}</span>
    </div>
  );
}

function Layout() {
  return (
    <div>
      <ScrollToTop />

      <Helmet>
        <meta charSet="utf-8" />
        <title>CodePerfect 95</title>
      </Helmet>

      <div className="">
        <div className="pt-4 lg:pt-8 px-4 pb-4 flex justify-between items-center w-full md:max-w-screen-lg md:mx-auto text-lg">
          <A
            link
            href="/"
            className="font-bold text-lg text-black no-underline whitespace-nowrap flex items-center"
          >
            <img
              alt="logo"
              className="w-auto h-12 inline-block mr-3"
              src={asset("/logo.png")}
            />
            <span className="hidden md:inline-block">CodePerfect 95</span>
          </A>
          <div className="flex items-baseline space-x-6">
            <A
              href={LINKS.docs}
              className="text-black no-underline whitespace-nowrap hidden md:inline-block"
            >
              Docs
            </A>
            <A
              href={LINKS.changelog}
              className="text-black no-underline whitespace-nowrap hidden md:inline-block"
            >
              Changelog
            </A>
            <A
              link
              href="/buy"
              className="text-black no-underline whitespace-nowrap hidden md:inline-block"
            >
              Buy
            </A>
            <A link href="/download" className="button main-button">
              <Icon className="mr-2" icon={HiOutlineDownload} />
              Download
            </A>
          </div>
        </div>
        <div>
          <Outlet />
        </div>
        <div
          className={cx(
            "px-4 pt-4 mb-8 lg:pt-8 lg:mb-12 flex flex-col md:flex-row md:items-start justify-center gap-x-8",
            "mr-4 border-r-100 text-gray-500"
          )}
        >
          <span>&copy; {new Date().getFullYear()} CodePerfect 95</span>
          <A link href="/buy" className="text-gray-500 no-underline md:hidden">
            Buy
          </A>
          <A
            link
            href="/download"
            className="text-gray-500 no-underline md:hidden"
          >
            Download
          </A>
          <A className="text-gray-500 no-underline" href={LINKS.docs}>
            Docs
          </A>
          <A className="text-gray-500 no-underline" href={LINKS.changelog}>
            Changelog
          </A>
          <A
            className="text-gray-500 no-underline"
            href={`mailto:${SUPPORT_EMAIL}`}
          >
            Support
          </A>
          <A link href="/terms" className="text-gray-500 no-underline">
            Terms &amp; Privacy
          </A>
        </div>
      </div>
    </div>
  );
}

function App() {
  return (
    <BrowserRouter>
      <Routes>
        <Route path="/" element={<Layout />}>
          <Route path="download" element={<Download />} />
          <Route path="buy" element={<BuyLicense />} />
          <Route path="payment-done" element={<PaymentDone />} />
          <Route path="portal-done" element={<PortalDone />} />
          <Route path="terms" element={<Terms />} />
          <Route path="privacy" element={<Navigate to="terms" />} />
          <Route path="/" element={<Home />} />
          <Route path="*" element={<Navigate to="/" />} />
        </Route>
      </Routes>
    </BrowserRouter>
  );
}

export default App;
