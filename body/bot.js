const mineflayer = require('mineflayer')
const { Vec3 } = require('vec3')
const zmq = require('zeromq')

// ── ZMQ Sockets ─────────────────────────────────────────────────
const pubSock = new zmq.Publisher()   // PUB: percepts → Head (port 5555)
const pullSock = new zmq.Pull()       // PULL: actions ← Head (port 5556)

const PUB_ENDPOINT = 'tcp://127.0.0.1:5555'
const PULL_ENDPOINT = 'tcp://127.0.0.1:5556'

// ── Mineflayer Bot ──────────────────────────────────────────────
const bot = mineflayer.createBot({
  host: 'localhost',
  port: 25565,
  username: 'Prometheus',
  auth: 'offline' // Matches online-mode=false in server.properties
})

bot.on('spawn', () => {
  console.log('Prometheus has opened its eyes (Spawned).')
  bot.chat('I am awake.')
  startPerceptPublisher()
})

bot.on('chat', (username, message) => {
  if (username === bot.username) return
  console.log(`[${username}]: ${message}`)
})

bot.on('kicked', console.log)
bot.on('error', console.log)

// ── Percept Publisher (100 ms interval) ─────────────────────────
function startPerceptPublisher () {
  setInterval(() => {
    if (!bot.entity) return // not spawned yet

    const nearbyEntities = []
    for (const entity of Object.values(bot.entities)) {
      if (entity === bot.entity) continue
      if (!entity.position) continue
      const dist = bot.entity.position.distanceTo(entity.position)
      if (dist > 32) continue // only within 32 blocks
      nearbyEntities.push({
        name: entity.name || entity.username || 'unknown',
        distance: Math.round(dist * 10) / 10,
        hostile: isHostile(entity)
      })
    }

    // Sort by distance
    nearbyEntities.sort((a, b) => a.distance - b.distance)

    const percept = {
      type: 'percept',
      health: bot.health,
      food: bot.food,
      position: {
        x: Math.round(bot.entity.position.x * 10) / 10,
        y: Math.round(bot.entity.position.y * 10) / 10,
        z: Math.round(bot.entity.position.z * 10) / 10
      },
      nearby_entities: nearbyEntities.slice(0, 10), // cap at 10
      ground: bot.entity.onGround ? 'safe' : 'airborne'
    }

    const msg = JSON.stringify(percept)
    pubSock.send(msg).catch(() => {}) // fire-and-forget
  }, 100)
}

// ── Hostile Mob Detection ───────────────────────────────────────
const HOSTILE_MOBS = new Set([
  'zombie', 'skeleton', 'creeper', 'spider', 'enderman',
  'witch', 'slime', 'phantom', 'drowned', 'husk', 'stray',
  'blaze', 'ghast', 'magma_cube', 'hoglin', 'piglin_brute',
  'warden', 'vindicator', 'evoker', 'ravager', 'vex',
  'pillager', 'guardian', 'elder_guardian', 'wither_skeleton',
  'cave_spider'
])

function isHostile (entity) {
  if (!entity.name) return false
  return HOSTILE_MOBS.has(entity.name.toLowerCase())
}

// ── Action Receiver (async PULL loop) ───────────────────────────
async function startActionReceiver () {
  for await (const [msg] of pullSock) {
    const text = msg.toString()
    console.log('[HEAD] <<', text)

    let action
    try {
      action = JSON.parse(text)
    } catch (e) {
      console.warn('[HEAD] Invalid JSON:', e.message)
      continue
    }

    dispatchAction(action)
  }
}

function dispatchAction (action) {
  switch (action.action) {
    case 'flee':
      actionFlee(action)
      break
    case 'eat':
      actionEat(action)
      break
    case 'idle':
      // no-op
      break
    case 'explore':
      actionExplore(action)
      break
    default:
      console.warn(`[HEAD] Unknown action: ${action.action}`)
  }
}

// ── Action Handlers ─────────────────────────────────────────────
function actionFlee (action) {
  if (!bot.entity) return
  const hostile = bot.nearestEntity(e => isHostile(e))
  if (!hostile) {
    console.log('[FLEE] No hostile found, standing still.')
    return
  }

  // Move away from the hostile entity
  const dx = bot.entity.position.x - hostile.position.x
  const dz = bot.entity.position.z - hostile.position.z
  const dist = Math.sqrt(dx * dx + dz * dz) || 1
  const fleeX = bot.entity.position.x + (dx / dist) * 10
  const fleeZ = bot.entity.position.z + (dz / dist) * 10

  const goal = { x: fleeX, y: bot.entity.position.y, z: fleeZ }
  console.log(`[FLEE] Running away from ${hostile.name} toward (${goal.x.toFixed(1)}, ${goal.z.toFixed(1)})`)

  // Simple movement — walk toward flee point
  bot.lookAt(new Vec3(goal.x, goal.y + 1.6, goal.z), false)
  bot.setControlState('forward', true)
  setTimeout(() => bot.setControlState('forward', false), 2000)
}

function actionEat (_action) {
  // Find food in inventory and eat it
  const foodItem = bot.inventory.items().find(item => {
    return item.name.includes('bread') ||
           item.name.includes('cooked') ||
           item.name.includes('apple') ||
           item.name.includes('steak') ||
           item.name.includes('carrot') ||
           item.name.includes('potato')
  })

  if (!foodItem) {
    console.log('[EAT] No food in inventory.')
    return
  }

  console.log(`[EAT] Eating ${foodItem.name}`)
  bot.equip(foodItem, 'hand')
    .then(() => bot.activateItem())
    .catch(e => console.warn('[EAT] Error:', e.message))
}

function actionExplore (_action) {
  if (!bot.entity) return

  // Walk to a random nearby position
  const angle = Math.random() * 2 * Math.PI
  const dist = 10 + Math.random() * 20
  const goalX = bot.entity.position.x + Math.cos(angle) * dist
  const goalZ = bot.entity.position.z + Math.sin(angle) * dist

  console.log(`[EXPLORE] Walking toward (${goalX.toFixed(1)}, ${goalZ.toFixed(1)})`)

  bot.lookAt(new Vec3(goalX, bot.entity.position.y + 1.6, goalZ), false)
  bot.setControlState('forward', true)
  setTimeout(() => bot.setControlState('forward', false), 3000)
}

// ── Bootstrap ZMQ ───────────────────────────────────────────────
async function initZmq () {
  await pubSock.bind(PUB_ENDPOINT)
  console.log(`[ZMQ] PUB bound to ${PUB_ENDPOINT}`)

  await pullSock.bind(PULL_ENDPOINT)
  console.log(`[ZMQ] PULL bound to ${PULL_ENDPOINT}`)

  startActionReceiver()
}

initZmq().catch(err => {
  console.error('[ZMQ] Failed to initialise:', err)
  process.exit(1)
})
