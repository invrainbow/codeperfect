import { AiOutlineCheck } from "@react-icons/all-files/ai/AiOutlineCheck";
import { AiOutlineClose } from "@react-icons/all-files/ai/AiOutlineClose";
import { BsCodeSlash } from "@react-icons/all-files/bs/BsCodeSlash";
import { FaLayerGroup } from "@react-icons/all-files/fa/FaLayerGroup";
import { FaPalette } from "@react-icons/all-files/fa/FaPalette";
import { FaRobot } from "@react-icons/all-files/fa/FaRobot";
import { GoPackage } from "@react-icons/all-files/go/GoPackage";
import { HiOutlineDownload } from "@react-icons/all-files/hi/HiOutlineDownload";
import { HiOutlineMail } from "@react-icons/all-files/hi/HiOutlineMail";
import { ImMagicWand } from "@react-icons/all-files/im/ImMagicWand";
import { IoMdSearch } from "@react-icons/all-files/io/IoMdSearch";
import { IoHardwareChipOutline } from "@react-icons/all-files/io5/IoHardwareChipOutline";
import { SiVim } from "@react-icons/all-files/si/SiVim";
import { VscTools } from "@react-icons/all-files/vsc/VscTools";
import cx from "classnames";
import React from "react";
import { Helmet } from "react-helmet";
import {
  BrowserRouter as Router,
  Link,
  Redirect,
  Route,
  Switch,
  useHistory,
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

let API_BASE = "https://api.codeperfect95.com";
if (process.env.NODE_ENV === "development") {
  API_BASE = "http://localhost:8080";
}

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
        "wall-of-text p-4 my-4 md:my-16 md:p-12 leading-normal md:mx-auto",
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
      className={cx(
        "text-center mb-6 text-3xl font-bold text-gray-800",
        className
      )}
      {...props}
    >
      {children}
    </h2>
  );
}

function Title({ className, ...props }) {
  return <H1 className={cx("text-4xl", className)} {...props} />;
}

/*
function H2({ className, children, ...props }) {
  return (
    <h2
      className={cx("text-xl leading-tight font-bold text-gray-700", className)}
      {...props}
    >
      {children}
    </h2>
  );
}
*/

function Icon({ noshift, icon: IconComponent, ...props }) {
  return (
    <span className="inline-block">
      <IconComponent {...props} />
    </span>
  );
}

function Feature({ bookend, name, className, children, icon, ...props }) {
  return (
    <div
      className={cx(
        className,
        "text-gray-600 p-3 md:p-5 rounded-lg relative",
        "bg-gray-100 transform md:hover:scale-105 ease-in duration-100"
      )}
      {...props}
    >
      <div className="flex flex-col mb-2">
        <div className="text-3xl mb-4 opacity-40">
          <Icon icon={icon} />
        </div>
        <div className="text-lg font-semibold leading-tight">{name}</div>
      </div>
      <div className="opacity-70">{children}</div>
    </div>
  );
}

function Home() {
  const [startDemo, setStartDemo] = React.useState(false);

  return (
    <div className="w-full">
      <div className="mt-8 px-4 sm:mt-24 sm:mb-8 md:mt-24 md:mb-12 lg:max-w-screen-xl lg:mx-auto">
        <div className="leading-tight text-center mb-2 text-4xl md:text-5xl font-bold text-black">
          <span className="inline-block">The Fastest</span>{" "}
          <span className="inline-block">IDE for Go</span>
        </div>
        <div className="mt-6 mb-6 md:mb-20 text-lg text-gray-400 text-center">
          <Link to="/download" className="button main-button py-4 px-8">
            <Icon className="mr-2" icon={HiOutlineDownload} />
            Download for Mac
          </Link>
        </div>
      </div>

      <div
        className={cx(
          "max-w-screen-lg mx-auto gap-8 px-3 items-center justify-center"
        )}
      >
        <div
          className="relative rounded-lg overflow-hidden border border-gray-800 shadow bg-black"
          style={startDemo ? { paddingBottom: "62.5%", top: 0 } : {}}
        >
          {startDemo ? (
            <iframe
              className="absolute top-0 left-0 w-full h-full"
              src="https://www.youtube-nocookie.com/embed/iPCfOoK9fco?autoplay=1"
              title="YouTube video player"
              frameborder="0"
              allow="fullscreen; accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture"
              allowfullscreen
            />
          ) : (
            <div
              className="cursor-pointer relative"
              onClick={() => setStartDemo(true)}
            >
              <div className="absolute w-full h-full flex items-center justify-center z-10">
                <img
                  alt="play"
                  style={{ width: "72px" }}
                  src={asset("/play.png")}
                />
              </div>
              <img
                className="block relative z-0"
                src={asset("/demo.png")}
                alt="demo"
              />
            </div>
          )}
        </div>
      </div>

      <div className="px-4 my-16 md:my-36">
        <div className="text-center font-bold text-3xl mb-8 text-black">
          A full-featured IDE, as fast as Sublime Text.
        </div>
        <div className="mx-auto max-w-screen-lg grid grid-cols-2 sm:grid-cols-3 gap-3 md:grid-cols-4 lg:grid-cols-5 md:gap-5">
          <Feature name="Code intelligence" icon={BsCodeSlash}>
            Autocomplete, go to definition, parameter hints, find usages, and
            more.
          </Feature>

          <Feature name="Built for speed" icon={IoHardwareChipOutline}>
            Written in C++, no Electron. Runs at 144 FPS. Keystrokes are
            instant.
          </Feature>

          <Feature name="Build and debug" icon={VscTools}>
            Edit, build and debug in one workflow, one app, one place.
          </Feature>

          <Feature name="Vim keybindings" icon={SiVim}>
            Vim keybindings work natively out of the box, the full feature set.
          </Feature>

          <Feature name="Instant fuzzy search" icon={IoMdSearch}>
            Works on files, symbols, commands, and completions.
          </Feature>

          <Feature name="Automatic refactoring" icon={ImMagicWand}>
            Rename, move, and generate code automatically.
          </Feature>

          <Feature name="Automatic import" icon={GoPackage}>
            Pull in libraries you need without moving your cursor.
          </Feature>

          <Feature name="Postfix macros" icon={FaRobot}>
            Generate code with macros that work intelligently on your Go
            expressions.
          </Feature>
          <Feature name="Native support for interfaces" icon={FaLayerGroup}>
            Navigate and generate interfaces in a few keystrokes.
          </Feature>
          <Feature name="Command Palette" icon={FaPalette}>
            Press ⌘K to run any command or action inside CodePerfect.
          </Feature>
        </div>
      </div>

      <div className="my-16 md:my-36 lg:mt-56 lg:mb-48 md:mx-4">
        <div className="md:rounded-md bg-black max-w-screen-xl mx-auto flex">
          <div className="lg:w-2/5 p-8 md:p-16">
            <div className="font-bold text-3xl text-white leading-snug">
              <div>Ready to get started?</div>
            </div>
            <div className="text-lg text-white opacity-80 mt-4 mb-6">
              <p>
                Try CodePerfect for free for 7 days with all features available.
              </p>
            </div>
            <div>
              <Link
                className="button main-button text-lg bg-white text-black hover:bg-white hover:text-black py-3 px-6"
                to="/download"
              >
                <Icon className="mr-3" icon={HiOutlineDownload} />
                Download for Mac
              </Link>
            </div>
          </div>
          <div className="flex-1 pl-12 relative hidden lg:block">
            <div
              style={{ transform: "translate(0, -47.5%)" }}
              className="absolute top-1/2 -left-16"
            >
              <img
                className="w-full h-auto border border-black rounded-lg shadow-md"
                src={asset("/beta.png")}
                alt="beta"
              />
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}

function Loading({ size = "80px", className, ...props }) {
  return (
    <div className={cx(className)} {...props}>
      <div className="lds-ring" style={{ width: size, height: size }}>
        {[0, 1, 2, 3].map((key) => (
          <div
            key={key}
            style={{
              width: `calc((${size} * 4) / 5)`,
              height: `calc((${size} * 4) / 5)`,
              margin: `calc(${size} / 10)`,
              borderWidth: `calc(${size} / 10)`,
            }}
          />
        ))}
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
      className={cx(className, "button download-button p-3 block text-center")}
      {...props}
    />
  );
}

function BuyLicenseBox({ className, ...props }) {
  return (
    <div
      className={cx(
        className,
        "w-auto overflow-hidden border border-r p-6 rounded-md"
      )}
      {...props}
    />
  );
}

function BuyLicense() {
  return (
    <WallOfText width="3xl">
      <Title>Buy License</Title>
      <div className="mt-8 grid grid-cols-1 sm:grid-cols-2 gap-4">
        <BuyLicenseBox>
          <h1 className="font-bold text-gray-700 text-sm uppercase mb-2">
            Personal
          </h1>
          <PricingPoint label="Commercial use allowed" />
          <PricingPoint label="All features unlocked" />
          <PricingPoint not label="Company can't pay" />
          <PricingPoint not label="Purchase can't be expensed" />

          <div className="grid grid-cols-2 gap-3 mt-5 pt-5 border-t">
            <div>
              <div className="flex items-center mb-2.5">
                <div className="leading-none text-xl font-bold text-gray-700">
                  $5
                </div>
                <div className="leading-none text-xs ml-1">per month</div>
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
                <div className="leading-none text-xs ml-1">per year</div>
              </div>
              <BuyLicenseButton href={LINKS.buyPersonalYearly}>
                Buy Yearly
              </BuyLicenseButton>
            </div>
          </div>
        </BuyLicenseBox>
        <BuyLicenseBox>
          <h1 className="font-bold text-gray-700 text-sm uppercase mb-2">
            Professional
          </h1>
          <PricingPoint label="Commercial use allowed" />
          <PricingPoint label="All features unlocked" />
          <PricingPoint label="Company can pay" />
          <PricingPoint label="Purchase can be expensed" />

          <div className="grid grid-cols-2 gap-3 mt-5 pt-5 border-t">
            <div>
              <div className="flex items-center mb-2.5">
                <div className="leading-none text-xl font-bold text-gray-700">
                  $10
                </div>
                <div className="leading-none text-xs ml-1">per month</div>
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
                <div className="leading-none text-xs ml-1">per year</div>
              </div>
              <BuyLicenseButton href={LINKS.buyProfessionalYearly}>
                Buy Yearly
              </BuyLicenseButton>
            </div>
          </div>
        </BuyLicenseBox>
      </div>
      <p>
        We'll send your license key to the email you provide during checkout. If
        you want multiple licenses, priority support, or any other custom
        requests fulfilled, please{" "}
        <a href={`mailto:${SUPPORT_EMAIL}`}>contact support</a>.
      </p>
    </WallOfText>
  );
}

function Download() {
  const [url, setUrl] = React.useState(null);
  const [err, setErr] = React.useState(false);
  const history = useHistory();

  React.useEffect(() => {
    async function run() {
      let dlurl = null;
      try {
        const resp = await fetch(`${API_BASE}/download`);
        const data = await resp.json();
        dlurl = data.url;
      } catch (err) {}

      if (!dlurl) {
        setErr(true);
      } else {
        setUrl(dlurl);
      }
    }
    run();
  }, [history]);

  return (
    <div className="flex items-center flex-col md:flex-row max-w-screen-xl px-4 mx-auto my-16 md:my-16 md:gap-4 lg:gap-8">
      <div className="max-w-sm lg:pb-12">
        <div>
          <span className="relative inline-block">
            <span className="text-3xl font-bold text-black">
              CodePerfect for Mac
            </span>
            <span
              className={cx(
                "absolute left-0 bottom-full text-xs uppercase bg-yellow-100 text-yellow-500",
                "px-2 py-0.5 rounded-full font-bold",
                "mb-0.5"
              )}
            >
              Public beta
            </span>
          </span>
        </div>
        <p>Try CodePerfect for free for 7 days with all features available.</p>
        <div className="lg:h-32">
          <div className="my-6">
            {url ? (
              <>
                <p className="mb-2">
                  <A
                    href={url || "#"}
                    className="button main-button inline-flex items-center justify-center"
                  >
                    <Icon className="mr-1" icon={HiOutlineDownload} />
                    <span>CodePerfect for Mac</span>
                  </A>
                </p>
                <p className="text-xs text-gray-400" style={{ marginTop: 0 }}>
                  Universal binary supports both Intel and Apple Silicon.
                </p>
              </>
            ) : err ? (
              <div className="p-4 bg-yellow-100 leading-none rounded text-yellow-700 opacity-70">
                Unable to fetch download link.
              </div>
            ) : (
              <Loading size="2em" />
            )}
          </div>
          <p>
            <A
              className="inline-block border-b-2 border-gray-600 no-underline leading-none font-semibold"
              href="https://docs.codeperfect95.com/docs/getting-started/"
              style={{ paddingBottom: "2px" }}
            >
              Getting Started
            </A>{" "}
            /{" "}
            <Link
              className="inline-block border-b-2 border-gray-600 no-underline leading-none font-semibold"
              to="/buy-license"
              style={{ paddingBottom: "2px" }}
            >
              Buy License
            </Link>
          </p>
        </div>
      </div>
      <div className="flex-1 hidden md:block">
        <div
          className="relative bg-black overflow-hidden rounded-lg shadow-md border-gray-700 border"
          style={{ paddingTop: "60.8173%" }}
        >
          <img
            className="absolute left-0 top-0 h-full"
            src={asset("/beta.png")}
            alt="beta"
          />
        </div>
      </div>
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

function PricingBox({
  title,
  monthly,
  yearly,
  isYearly,
  buyLicense,
  children,
  cta,
  className,
  premium,
}) {
  return (
    <div
      className={cx(
        className,
        "w-auto rounded-lg overflow-hidden",
        "border border-gray-200 shadow-sm flex flex-col justify-between"
      )}
    >
      <div className="p-6">
        <h1 className="font-bold text-gray-600 text-sm uppercase mb-0.5">
          {title}
        </h1>
        <div className="leading-none">
          {premium ? (
            <div className="font-bold text-black text-2xl">Contact us</div>
          ) : buyLicense ? (
            <div className="flex space-x-4">
              <div className="flex items-end">
                <div className="font-bold text-black text-3xl">${monthly}</div>
                <div className="leading-tight text-xs pb-1.5 ml-1">
                  {" "}
                  per month
                </div>
              </div>
              <div className="flex items-end">
                <div className="font-bold text-black text-3xl">${yearly}</div>
                <div className="leading-tight text-xs pb-1.5 ml-1">
                  {" "}
                  per year
                </div>
              </div>
            </div>
          ) : (
            <div className="flex items-end">
              <div className="font-bold text-black text-3xl">
                ${isYearly ? yearly : monthly}
              </div>
              <div className="leading-tight text-xs pb-1.5 ml-1">
                <div>per {isYearly ? "year" : "month"}</div>
              </div>
            </div>
          )}
        </div>
        <div className="text-gray-500 text-left mt-4">{children}</div>
      </div>
      <div className="p-6 border-t border-gray-200">{cta}</div>
    </div>
  );
}

function PricingPoint({ label, not }) {
  return (
    <div
      className={cx(
        "flex items-start space-x-1 leading-5 mb-1 last:mb-0",
        not && "text-red-600"
      )}
    >
      <Icon
        className="relative top-1"
        icon={not ? AiOutlineClose : AiOutlineCheck}
      />
      <span>{label}</span>
    </div>
  );
}

function Anchor({ name }) {
  const ref = React.useCallback(
    (elem) => {
      if (elem) {
        if (window.location.hash.slice(1) === name) {
          setTimeout(() => elem.scrollIntoView(), 1);
        }
      }
    },
    [name]
  );

  // eslint-disable-next-line
  return <a ref={ref} name={name}></a>;
}

function Pricing() {
  const [yearly, setYearly] = React.useState(false);

  return (
    <div className="pricing my-20">
      <Anchor name="pricing" />
      <H1 className="text-4xl">Pricing</H1>
      <div className="text-center">
        <span className="relative inline-block text-sm">
          <button
            onClick={() => setYearly(false)}
            className={cx(
              "left-0 inline-block rounded-full py-1 px-3 font-medium cursor-pointer outline-none",
              !yearly ? "bg-gray-200 text-black" : "text-gray-400"
            )}
          >
            Monthly
          </button>
          <button
            onClick={() => setYearly(true)}
            className={cx(
              "left-0 inline-block rounded-full py-1 px-4 font-medium cursor-pointer outline-none",
              yearly ? "bg-gray-200 text-black" : "text-gray-400"
            )}
          >
            Annual
          </button>
        </span>
      </div>
      <div className="mt-8">
        <div
          className={cx(
            "max-w-5xl mx-auto",
            "px-4 md:px-12",
            "grid grid-cols-1 md:grid-cols-3 gap-4 lg:gap-8"
          )}
        >
          <PricingBox
            title="Personal"
            monthly={5}
            yearly={50}
            isYearly={yearly}
            cta={
              <div className="grid grid-cols-1 gap-2">
                <Link
                  className="button main-button flex gap-1.5 items-center justify-center"
                  to="/download"
                >
                  Get Started
                </Link>
                <Link
                  className="button download-button block text-center"
                  to="/buy-license"
                >
                  Buy License
                </Link>
              </div>
            }
          >
            <PricingPoint label="7-day free trial" />
            <PricingPoint label="All features unlocked" />
            <PricingPoint not label="Company can't pay" />
            <PricingPoint not label="Purchase can't be expensed" />
          </PricingBox>
          <PricingBox
            title="Professional"
            monthly={10}
            yearly={100}
            isYearly={yearly}
            cta={
              <div className="grid grid-cols-1 gap-2">
                <Link
                  className="button main-button flex gap-1.5 items-center justify-center"
                  to="/download"
                >
                  Get Started
                </Link>
                <Link
                  className="button download-button block text-center"
                  to="/buy-license"
                >
                  Buy License
                </Link>
              </div>
            }
          >
            <PricingPoint label="All features in Personal" />
            <PricingPoint label="Company can pay" />
            <PricingPoint label="Purchase can be expensed" />
          </PricingBox>
          <PricingBox
            title="Custom"
            premium
            cta={
              <div className="grid grid-cols-1 gap-2">
                <a
                  className="button main-button flex gap-1.5 items-center justify-center"
                  href="mailto:support@codeperfect95.com"
                >
                  <Icon icon={HiOutlineMail} />
                  Contact Support
                </a>
                <div className="button download-button block text-center opacity-0 select-none">
                  spacer
                </div>
              </div>
            }
          >
            <PricingPoint label="All features in Professional" />
            <PricingPoint label="Priority support" />
            <PricingPoint label="Custom requests" />
            <PricingPoint label="Multiple licenses for team" />
          </PricingBox>
        </div>
      </div>

      <div className="mt-16 md:mt-24 sm:mx-4">
        <div className="sm:rounded-md bg-black max-w-screen-lg mx-auto flex">
          <div className="lg:w-1/2 p-8 md:p-16 mx-auto text-center">
            <div className="font-bold text-3xl text-white leading-snug">
              <div>Ready to get started?</div>
            </div>
            <div className="text-xl text-white opacity-80 mt-4 mb-6">
              <p>
                Try CodePerfect for free for 7 days with all features available.
              </p>
            </div>
            <div>
              <Link
                className="button main-button text-lg bg-white text-black hover:bg-white hover:text-black py-3 px-6"
                to="/download"
              >
                <Icon className="mr-3" icon={HiOutlineDownload} />
                Download for Mac
              </Link>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}

function Countdown() {
  const [showLeft, setShowLeft] = React.useState(false);
  const weeksPassed = (new Date() - new Date(2022, 1, 2)) / 86400000 / 7;

  return (
    <div className="fixed top-0 left-0 right-0 bottom-0 z-50 bg-white">
      <div className="px-4 my-48 max-w-screen-md mx-auto">
        <div
          className={cx(
            "mt-12 h-5 rounded border-2 relative",
            showLeft
              ? "border-red-700 bg-red-100"
              : "border-green-700 bg-green-100"
          )}
        >
          <div
            className={cx(
              "absolute top-0 bottom-0",
              showLeft ? "right-0 bg-red-700" : "left-0 bg-green-700"
            )}
            style={{
              width: `${
                ((showLeft ? 104 - weeksPassed : weeksPassed) / 104) * 100
              }%`,
            }}
          />
        </div>
        <div className="text-center mt-4 flex justify-between">
          <div>
            <button
              className="text-black font-bold"
              onClick={() => setShowLeft(!showLeft)}
              style={{ fontVariantNumeric: "tabular-nums" }}
            >
              {!showLeft ? (
                <span className="text-green-700">
                  {Math.floor(weeksPassed)}/104 weeks done
                </span>
              ) : (
                <span className="text-red-700">
                  {104 - Math.floor(weeksPassed)}/104 weeks left
                </span>
              )}
            </button>
          </div>
          <div className="text-black font-bold">
            {((weeksPassed / 104) * 100).toFixed(2)}%
          </div>
        </div>
      </div>
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

      <div className="text-gray-500">
        <div className="pt-4 lg:pt-8 px-4 pb-4 flex justify-between items-center w-full lg:max-w-screen-xl lg:mx-auto">
          <Link
            to="/"
            className="font-bold text-lg text-black no-underline whitespace-nowrap flex items-center"
          >
            <img
              alt="logo"
              className="w-auto h-16 inline-block mr-4"
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
              to="/pricing"
              className="text-black no-underline whitespace-nowrap hidden md:inline-block"
            >
              Pricing
            </Link>
            <Link
              to="/buy-license"
              className="text-black no-underline whitespace-nowrap hidden md:inline-block"
            >
              Buy License
            </Link>
            <Link to="/download" className="button main-button">
              <Icon className="mr-2" icon={HiOutlineDownload} />
              Download
            </Link>
          </div>
        </div>
        <div>
          <Switch>
            <Route path="/countdown" exact>
              <Countdown />
            </Route>
            <Route path="/download" exact>
              <Redirect to="/download/mac" />
            </Route>
            <Route path="/download/mac" exact>
              <Download />
            </Route>
            <Route path="/buy-license" exact>
              <BuyLicense />
            </Route>
            <Route path="/payment-done" exact>
              <PaymentDone />
            </Route>
            <Route path="/portal-done" exact>
              <PortalDone />
            </Route>
            <Route path="/pricing" exact>
              <Pricing />
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
            "px-4 pt-4 mb-8 lg:pt-8 lg:mb-12 flex flex-col justify-between",
            "lg:max-w-screen-xl lg:mx-auto sm:flex-row border-t sm:border-none"
          )}
        >
          <div className="text-gray-500">
            &copy; {new Date().getFullYear()} CodePerfect 95
          </div>
          <div className="flex flex-col sm:flex-row space-x-0 mt-2 sm:mt-0">
            <div className="sm:flex sm:flex-row sm:space-x-12 md:space-x-16">
              <div>
                <div>
                  <Link className="text-gray-500 no-underline" to="/download">
                    Download
                  </Link>
                </div>
                <div>
                  <Link className="text-gray-500 no-underline" to="/pricing">
                    Pricing
                  </Link>
                </div>
                <div>
                  <Link
                    className="text-gray-500 no-underline"
                    to="/buy-license"
                  >
                    Buy License
                  </Link>
                </div>
              </div>
              <div>
                <div>
                  <A className="text-gray-500 no-underline" href={LINKS.docs}>
                    Docs
                  </A>
                </div>
                <div>
                  <A
                    className="text-gray-500 no-underline"
                    href={LINKS.changelog}
                  >
                    Changelog
                  </A>
                </div>
              </div>
              <div>
                <div>
                  <A
                    className="text-gray-500 no-underline"
                    href={`mailto:${SUPPORT_EMAIL}`}
                  >
                    Support
                  </A>
                </div>
                <div>
                  <Link to="/terms" className="text-gray-500 no-underline">
                    Terms &amp; Privacy
                  </Link>
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>
    </Router>
  );
}

export default App;
