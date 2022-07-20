import { AiFillApple } from "@react-icons/all-files/ai/AiFillApple";
import { AiFillWindows } from "@react-icons/all-files/ai/AiFillWindows";
import { AiOutlineCheck } from "@react-icons/all-files/ai/AiOutlineCheck";
import { AiOutlineClose } from "@react-icons/all-files/ai/AiOutlineClose";
import { BsCodeSlash } from "@react-icons/all-files/bs/BsCodeSlash";
import { FaLayerGroup } from "@react-icons/all-files/fa/FaLayerGroup";
import { FaLinux } from "@react-icons/all-files/fa/FaLinux";
import { FaPalette } from "@react-icons/all-files/fa/FaPalette";
import { FaRobot } from "@react-icons/all-files/fa/FaRobot";
import { GoPackage } from "@react-icons/all-files/go/GoPackage";
import { HiOutlineDownload } from "@react-icons/all-files/hi/HiOutlineDownload";
import { ImMagicWand } from "@react-icons/all-files/im/ImMagicWand";
import { IoMdSearch } from "@react-icons/all-files/io/IoMdSearch";
import { SiVim } from "@react-icons/all-files/si/SiVim";
import { VscTools } from "@react-icons/all-files/vsc/VscTools";

import cx from "classnames";
import React from "react";
import { Helmet } from "react-helmet";
import { useMediaQuery } from "react-responsive";
import {
  BrowserRouter as Router,
  Link,
  Redirect,
  Route,
  Switch,
  useLocation,
} from "react-router-dom";
import "./index.scss";

const SUPPORT_EMAIL = "support@codeperfect95.com";

const LINKS = {
  // random links to docs
  docs: "https://docs.codeperfect95.com",
  gettingStarted: "https://docs.codeperfect95.com/getting-started",
  changelog: "https://docs.codeperfect95.com/changelog",

  // stripe links
  buyPersonalMonthly: "https://buy.stripe.com/aEU5kx2aTaso4TK008",
  buyPersonalYearly: "https://buy.stripe.com/fZefZb2aTdEAbi8aEN",
  buyProfessionalMonthly: "https://buy.stripe.com/6oE28ldTB5843PG9AK",
  buyProfessionalYearly: "https://buy.stripe.com/28o8wJ3eXfMI4TK5kv",
};

const CDN_PATH = "https://d2mje1xp79ofdv.cloudfront.net";

function asset(path) {
  const dev = process.env.NODE_ENV === "development";
  return dev ? `${path}` : `${CDN_PATH}${path}`;
}

function A({ children, ...props }) {
  return (
    <a target="_blank" rel="noreferrer" {...props}>
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
    name: "Code intelligence",
    icon: BsCodeSlash,
    description:
      "Autocomplete, go to definition, parameter hints, find usages, and more.",
    videoId: "r9eQth4Q5jg",
  },
  {
    name: "Build and debug",
    icon: VscTools,
    description: "Edit, build and debug in one workflow, one app, one place.",
    videoId: "GC-0tCy4P1U",
  },
  {
    name: "Vim keybindings",
    icon: SiVim,
    description:
      "Vim keybindings work natively out of the box, the full feature set.",
    videoId: "GKYCA3UsmrU",
  },
  {
    name: "Instant fuzzy search",
    icon: IoMdSearch,
    description: "Works on files, symbols, commands, and completions.",
    videoId: "_xLgr6Ng4qQ",
  },
  {
    name: "Automatic refactoring",
    icon: ImMagicWand,
    description: "Rename, move, and generate code automatically.",
    videoId: "YDmD7RKX6wI",
  },
  {
    name: "Automatic import",
    icon: GoPackage,
    description: "Pull in libraries you need without moving your cursor.",
    videoId: "r9eQth4Q5jg",
  },
  {
    name: "Postfix macros",
    icon: FaRobot,
    description:
      "Generate code with macros that work intelligently on your Go expressions.",
    videoId: "GC-0tCy4P1U",
  },
  {
    name: "Native interface support",
    icon: FaLayerGroup,
    description: "Navigate and generate interfaces in a few keystrokes.",
    videoId: "GKYCA3UsmrU",
  },
  {
    name: "Command Palette",
    icon: FaPalette,
    description: "Press ⌘K to run any command or action inside CodePerfect.",
    videoId: "_xLgr6Ng4qQ",
  },
];

function YoutubeEmbed({ videoId, first }) {
  return (
    <div
      className="relative h-0 rounded-lg border-2 border-black overflow-hidden shadow-md"
      style={{ paddingBottom: "56.25%" }}
    >
      <iframe
        className="absolute t-0 l-0 w-full h-full"
        src={`https://www.youtube.com/embed/${videoId}?autoplay=${
          first ? 0 : 1
        }`}
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
        {FEATURES.map((it) => (
          <button
            className={cx(
              "text-left block w-full rounded-md relative mb-1 overflow-hidden",
              selected.name === it.name ? "opacity-100" : "opacity-70"
            )}
            style={{
              background:
                selected.name === it.name
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
                <div className="text-xl leading-none opacity-60 text-black">
                  <Icon block icon={it.icon} />
                </div>
                <div
                  className={cx(
                    "font-semibold leading-none relative",
                    selected.name === it.name ? "text-black" : "text-gray-700"
                  )}
                  style={{ paddingTop: "-1px" }}
                >
                  {it.name}
                </div>
              </div>
              <div
                className={cx(
                  "text-sm text-gray-700 overflow-hidden transition-all duration-100 ease-linear",
                  selected.name === it.name ? "mt-1.5 max-h-16" : "mt-0 max-h-0"
                )}
              >
                {it.description}
              </div>
            </div>
            {selected &&
              selected.name === it.name &&
              selected.videoId &&
              !isbig && (
                <YoutubeEmbed videoId={selected.videoId} first={first} />
              )}
          </button>
        ))}
      </div>
      {selected && selected.videoId && isbig && (
        <YoutubeEmbed videoId={selected.videoId} first={first} />
      )}
    </div>
  );
}

function FeaturesGrid() {
  return (
    <div className="grid grid-cols-1 md:grid-cols-3 w-full md:max-w-screen-lg mx-auto gap-4 md:gap-6 text-sm">
      {FEATURES.map((it) => (
        <div className="text-left block w-full rounded-md overflow-hidden p-4 bg-gray-100">
          <div className="flex flex-row items-center gap-x-2">
            <div className="text-xl leading-none opacity-60 text-black">
              <Icon block icon={it.icon} />
            </div>
            <div
              className="font-semibold leading-none relative text-gray-800"
              style={{ paddingTop: "-1px" }}
            >
              {it.name}
            </div>
          </div>
          <div className="text-sm text-gray-700 mt-2">{it.description}</div>
        </div>
      ))}
    </div>
  );
}

function Home() {
  return (
    <div className="my-8 md:my-20 w-full">
      <div className="max-w-full md:max-w-xl md:mx-auto text-lg md:text-xl leading-relaxed mb-16 md:mb-24 p-4">
        <div className="text-center font-bold text-3xl md:text-4xl mb-8 text-black">
          A High Performance IDE for Go
        </div>
        <p>
          Cross-platform and written in C++, CodePerfect indexes large codebases
          quickly and responds to every user input in 16ms.
        </p>
        <p>
          Built for Vim users who want more power, Jetbrains users who want more
          speed, and everyone in between.
        </p>

        <div className="mt-8 text-center">
          <Link
            to="/download"
            className="button main-button justify-center flex md:inline-flex text-center"
          >
            <Icon className="mr-2" icon={HiOutlineDownload} />
            Download
          </Link>
        </div>
        <div className="text-center text-3xl mt-4 font-medium text-gray-400">
          <Icon icon={AiFillWindows} /> <Icon icon={AiFillApple} />{" "}
          <Icon icon={FaLinux} />
        </div>
      </div>

      <div className="px-4">
        <div className="mb-8 font-bold text-black text-2xl text-center">
          A full-featured IDE, as fast as Sublime Text.
        </div>
        {/*
        <VideoPlayer />
        <br />
        */}
        <FeaturesGrid />
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
                <BuyLicenseButton
                  {...disableButtonProps}
                  href={LINKS.buyPersonalMonthly}
                >
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
                <BuyLicenseButton
                  {...disableButtonProps}
                  href={LINKS.buyPersonalYearly}
                >
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
                <BuyLicenseButton
                  {...disableButtonProps}
                  href={LINKS.buyProfessionalMonthly}
                >
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
                <BuyLicenseButton
                  {...disableButtonProps}
                  href={LINKS.buyProfessionalYearly}
                >
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

function DownloadSupportingLink({ className, link, href, label }) {
  const ourClassName = cx(
    "inline-block border-b-2 border-gray-600",
    "no-underline leading-none font-semibold",
    className
  );
  const props = {
    className: ourClassName,
    style: { paddingBottom: "2px" },
  };
  let Component;
  if (link) {
    props.to = href;
    Component = Link;
  } else {
    props.href = href;
    Component = A;
  }

  return (
    <div>
      <Component {...props}>{label}</Component>
    </div>
  );
}

function Download() {
  const url = "/";

  const links = [
    { icon: AiFillWindows, label: "Windows", url },
    { icon: AiFillApple, label: "macOS - Intel", url },
    { icon: AiFillApple, label: "macOS - M1", url },
    { icon: FaLinux, label: "Linux", url },
  ];

  const supportingLinks = (
    <p className="flex flex-col w-full md:flex-row gap-2 md:gap-4">
      <DownloadSupportingLink href="/buy" link label="Buy License" />
      <DownloadSupportingLink
        href="https://docs.codeperfect95.com/getting-started/"
        label="Getting Started"
      />
      <DownloadSupportingLink
        href="https://docs.codeperfect95.com/changelog/"
        label="Changelog"
      />
    </p>
  );

  return (
    <div className="flex items-center flex-col md:flex-row max-w-screen-md px-4 mx-auto my-16 md:my-28 gap-8 md:gap-4 lg:gap-8">
      <div className="max-w-full md:max-w-sm">
        <Title>Download CodePerfect</Title>
        <p>
          CodePerfect is free to evaluate for 7 days, with all features
          available. After the trial period you'll need a license for continued
          use.
        </p>
        <div className="mt-4 hidden md:block">{supportingLinks}</div>
      </div>
      <div className="w-full md:w-auto md:flex-1 border border-gray-200 bg-gray-50 rounded-md overflow-hidden">
        <h1 className="font-bold text-lg p-4 leading-none">Build 22.07</h1>
        <div className="grid grid-cols-1">
          {links.map((it) => (
            <A
              href={it.url}
              className="p-4 flex items-center border-t border-gray-200 bg-white no-underline hover:text-black leading-none"
              {...disableButtonProps}
            >
              <Icon className="mr-1" icon={it.icon} />
              {it.label}
            </A>
          ))}
        </div>
      </div>
      <div className="md:hidden w-full">{supportingLinks}</div>
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

function App() {
  return (
    <Router>
      <ScrollToTop />

      <Helmet>
        <meta charSet="utf-8" />
        <title>CodePerfect 95</title>
      </Helmet>

      <div className="">
        <div className="pt-4 lg:pt-8 px-4 pb-4 flex justify-between items-center w-full md:max-w-screen-lg md:mx-auto text-lg">
          <Link
            to="/"
            className="font-bold text-lg text-black no-underline whitespace-nowrap flex items-center"
          >
            <img
              alt="logo"
              className="w-auto h-12 inline-block mr-3"
              src={asset("/logo.png")}
            />
            <span className="hidden md:inline-block">CodePerfect 95</span>
          </Link>
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
            <Link
              to="/buy"
              className="text-black no-underline whitespace-nowrap hidden md:inline-block"
            >
              Buy
            </Link>
            <Link to="/download" className="button main-button">
              <Icon className="mr-2" icon={HiOutlineDownload} />
              Download
            </Link>
          </div>
        </div>
        <div>
          <Switch>
            <Route path="/download" exact>
              <Download />
            </Route>
            <Route path="/buy" exact>
              <BuyLicense />
            </Route>
            <Route path="/payment-done" exact>
              <PaymentDone />
            </Route>
            <Route path="/portal-done" exact>
              <PortalDone />
            </Route>
            <Route path="/terms">
              <Terms />
            </Route>
            <Route path="/privacy">
              <Redirect to="/terms" />
            </Route>
            <Route exact path="/">
              <Home />
            </Route>
            <Route>
              <Redirect to="/" />
            </Route>
          </Switch>
        </div>
        <div
          className={cx(
            "px-4 pt-4 mb-8 lg:pt-8 lg:mb-12 flex flex-col md:flex-row md:items-start justify-center gap-x-8",
            "mr-4 border-r-100 text-gray-500"
          )}
        >
          <span>&copy; {new Date().getFullYear()} CodePerfect 95</span>
          <Link to="/buy" className="text-gray-500 no-underline md:hidden">
            Buy
          </Link>
          <Link to="/download" className="text-gray-500 no-underline md:hidden">
            Download
          </Link>
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
          <Link to="/terms" className="text-gray-500 no-underline">
            Terms &amp; Privacy
          </Link>
        </div>
      </div>
    </Router>
  );
}

export default App;
